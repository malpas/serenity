#include "Parser.h"
#include <AK/Optional.h>
#include <AK/StringBuilder.h>
#include <stdio.h>
#include <unistd.h>

void Parser::commit_token()
{
    if (m_token.is_empty())
        return;
    if (m_state == InRedirectionPath) {
        m_redirections.last().path = String::copy(m_token);
        m_token.clear_with_capacity();
        return;
    }
    m_tokens.append(String::copy(m_token));
    m_token.clear_with_capacity();
};

void Parser::commit_subcommand()
{
    if (m_tokens.is_empty())
        return;
    m_subcommands.append({ move(m_tokens), move(m_redirections), {} });
}

void Parser::commit_command()
{
    if (m_subcommands.is_empty())
        return;
    m_commands.append({ move(m_subcommands) });
}

void Parser::do_pipe()
{
    m_redirections.append({ Redirection::Pipe, STDOUT_FILENO });
    commit_subcommand();
}

void Parser::begin_redirect_read(int fd)
{
    m_redirections.append({ Redirection::FileRead, fd });
}

void Parser::begin_redirect_write(int fd)
{
    m_redirections.append({ Redirection::FileWrite, fd });
}

Vector<Command> Parser::parse()
{
    auto subbed_input = process_substitutions(m_input);
    if (!subbed_input.has_value()) {
        fprintf(stderr, "Could not perform command substitution\n");
        return {};
    }
    m_input = subbed_input.value();
    for (int i = 0; i < m_input.length(); ++i) {
        char ch = m_input.characters()[i];
        switch (m_state) {
        case State::Free:
            if (ch == ' ') {
                commit_token();
                break;
            }
            if (ch == ';') {
                commit_token();
                commit_subcommand();
                commit_command();
                break;
            }
            if (ch == '|') {
                commit_token();
                if (m_tokens.is_empty()) {
                    fprintf(stderr, "Syntax error: Nothing before pipe (|)\n");
                    return {};
                }
                do_pipe();
                break;
            }
            if (ch == '>') {
                commit_token();
                begin_redirect_write(STDOUT_FILENO);

                // Search for another > for append.
                m_state = State::InWriteAppendOrRedirectionPath;
                break;
            }
            if (ch == '<') {
                commit_token();
                begin_redirect_read(STDIN_FILENO);
                m_state = State::InRedirectionPath;
                break;
            }
            if (ch == '\\') {
                if (i == m_input.length() - 1) {
                    fprintf(stderr, "Syntax error: Nothing to escape (\\)\n");
                    return {};
                }
                char next_ch = m_input.characters()[i + 1];
                m_token.append(next_ch);
                i += 1;
                break;
            }
            if (ch == '\'') {
                m_state = State::InSingleQuotes;
                break;
            }
            if (ch == '\"') {
                m_state = State::InDoubleQuotes;
                break;
            }
            m_token.append(ch);
            break;
        case State::InWriteAppendOrRedirectionPath:
            if (ch == '>') {
                commit_token();
                m_state = State::InRedirectionPath;
                ASSERT(m_redirections.size());
                m_redirections[m_redirections.size() - 1].type = Redirection::FileWriteAppend;
                break;
            }

            // Not another > means that it's probably a path.
            m_state = InRedirectionPath;
            [[fallthrough]];
        case State::InRedirectionPath:
            if (ch == '<') {
                commit_token();
                begin_redirect_read(STDIN_FILENO);
                m_state = State::InRedirectionPath;
                break;
            }
            if (ch == '>') {
                commit_token();
                begin_redirect_read(STDOUT_FILENO);
                m_state = State::InRedirectionPath;
                break;
            }
            if (ch == '|') {
                commit_token();
                if (m_tokens.is_empty()) {
                    fprintf(stderr, "Syntax error: Nothing before pipe (|)\n");
                    return {};
                }
                do_pipe();
                m_state = State::Free;
                break;
            }
            if (ch == ' ')
                break;
            m_token.append(ch);
            break;
        case State::InSingleQuotes:
            if (ch == '\'') {
                commit_token();
                m_state = State::Free;
                break;
            }
            m_token.append(ch);
            break;
        case State::InDoubleQuotes:
            if (ch == '\"') {
                commit_token();
                m_state = State::Free;
                break;
            }
            if (ch == '\\') {
                if (i == m_input.length() - 1) {
                    fprintf(stderr, "Syntax error: Nothing to escape (\\)\n");
                    return {};
                }
                char next_ch = m_input.characters()[i + 1];
                if (next_ch != '$' && next_ch != '`' && next_ch != '"' && next_ch != '\\' && next_ch != '\n') {
                    m_token.append(ch);
                    continue;
                }
                m_token.append(next_ch);
                i += 1;
                break;
            }
            m_token.append(ch);
            break;
        };
    }
    commit_token();
    commit_subcommand();
    commit_command();

    if (!m_subcommands.is_empty()) {
        for (auto& redirection : m_subcommands.last().redirections) {
            if (redirection.type == Redirection::Pipe) {
                fprintf(stderr, "Syntax error: Nothing after last pipe (|)\n");
                return {};
            }
        }
    }

    return move(m_commands);
}

// Escape shell-specific characters. Used when substituting outputs
// that contain special characters.
String escape_special(const String& string)
{
    StringBuilder escaped;
    for (int i = 0; i < string.length(); ++i) {
        char ch = string[i];
        if (ch == '<' || ch == '>' || ch == '|'
            || ch == '`') {
            escaped.append('\\');
        }
        escaped.append(ch);
    }
    return escaped.build();
}

// FIXME This can be optimised if the Parser::parse state machine is ever
// replaced with a recursive parsing algorithm. Right now, this is a
// pre-processing function that looks through the entire text it's given.
Optional<String> Parser::process_substitutions(const String& cmd)
{
    StringBuilder new_cmd;
    StringBuilder sub_cmd;
    int sub_level = 0;
    for (int i = 0; i < cmd.length(); ++i) {
        char ch = cmd[i];
        char prev_ch = i > 0 ? cmd[i - 1] : '\0';
        char next_ch = i < cmd.length() - 1 ? cmd[i + 1] : '\0';
        if (ch == '$' && next_ch == '(' && prev_ch != '\\') {
            if (sub_level > 0) {
                sub_cmd.append("$(");
            }
            ++sub_level;
            ++i;
            continue;
        }
        if (sub_level == 0) {
            new_cmd.append(ch);
            continue;
        }
        if (ch == ')') {
            --sub_level;
            if (sub_level > 0) {
                sub_cmd.append(')');
                continue;
            }
            StringBuilder output;
            auto recursed_cmd = process_substitutions(sub_cmd.build());
            if (!recursed_cmd.has_value())
                return Optional<String>();
            sub_cmd = StringBuilder();
            FILE* fp = popen(recursed_cmd.value().characters(), "r");
            char* line = NULL;
            size_t n;
            int read;
            while ((read = getline(&line, &n, fp)) != -1) {
                for (int i = 0; i < read; ++i) {
                    if (line[i] == '\n')
                        continue;
                    output.append(line[i]);
                }
                output.append(' ');
            }
            free(line);
            pclose(fp);
            auto final_output = output.build();
            final_output = final_output.substring(0, final_output.length() - 1);
            new_cmd.append(escape_special(final_output).characters());
            continue;
        }
        sub_cmd.append(ch);
    }
    if (sub_level > 0)
        return Optional<String>();
    return new_cmd.build();
}