#include "lexer.h"

#include <algorithm>
#include <charconv>
#include <unordered_map>

using namespace std;

namespace parse {

bool operator==(const Token& lhs, const Token& rhs) {
    using namespace token_type;

    if (lhs.index() != rhs.index()) {
        return false;
    }
    if (lhs.Is<Char>()) {
        return lhs.As<Char>().value == rhs.As<Char>().value;
    }
    if (lhs.Is<Number>()) {
        return lhs.As<Number>().value == rhs.As<Number>().value;
    }
    if (lhs.Is<String>()) {
        return lhs.As<String>().value == rhs.As<String>().value;
    }
    if (lhs.Is<Id>()) {
        return lhs.As<Id>().value == rhs.As<Id>().value;
    }
    return true;
}

bool operator!=(const Token& lhs, const Token& rhs) {
    return !(lhs == rhs);
}

std::ostream& operator<<(std::ostream& os, const Token& rhs) {
    using namespace token_type;

#define VALUED_OUTPUT(type) \
    if (auto p = rhs.TryAs<type>()) return os << #type << '{' << p->value << '}';

    VALUED_OUTPUT(Number);
    VALUED_OUTPUT(Id);
    VALUED_OUTPUT(String);
    VALUED_OUTPUT(Char);

#undef VALUED_OUTPUT

#define UNVALUED_OUTPUT(type) \
    if (rhs.Is<type>()) return os << #type;

    UNVALUED_OUTPUT(Class);
    UNVALUED_OUTPUT(Return);
    UNVALUED_OUTPUT(If);
    UNVALUED_OUTPUT(Else);
    UNVALUED_OUTPUT(Def);
    UNVALUED_OUTPUT(Newline);
    UNVALUED_OUTPUT(Print);
    UNVALUED_OUTPUT(Indent);
    UNVALUED_OUTPUT(Dedent);
    UNVALUED_OUTPUT(And);
    UNVALUED_OUTPUT(Or);
    UNVALUED_OUTPUT(Not);
    UNVALUED_OUTPUT(Eq);
    UNVALUED_OUTPUT(NotEq);
    UNVALUED_OUTPUT(LessOrEq);
    UNVALUED_OUTPUT(GreaterOrEq);
    UNVALUED_OUTPUT(None);
    UNVALUED_OUTPUT(True);
    UNVALUED_OUTPUT(False);
    UNVALUED_OUTPUT(Eof);

#undef UNVALUED_OUTPUT

    return os << "Unknown token :("sv;
}

Lexer::Lexer(std::istream& input)
    : token_id_(0) {

    using namespace token_type;

    int indent = 0;
    int cur_indent = 0;

    while (true) {
        if (!input) {
            break;
        }
        const char c = input.get();

        if ((tokens_.size() != 0) && (c != ' ') && (c != '\n') && tokens_.back().Is<Newline>()) {
            int delta = cur_indent - indent;

            if (delta >= 2) {
                for (int i = 0; i < delta / 2; ++i) {
                    tokens_.push_back(Dedent{});
                    cur_indent -= 2;
                }
            }
            indent = 0;
        }

        if (c >= '0' && c <= '9') {
            tokens_.push_back(LoadNumber(input.unget()));
        }
        else if (c == '_' || (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z')) {
            tokens_.push_back(LoadId(input.unget()));
        }
        else if (c == '\'' || c == '\"') {
            tokens_.push_back(LoadString(input, c));
        }
        else if ((tokens_.size() != 0) && c == ' ') {
            if (tokens_.back().Is<Newline>()) {
                if (++indent == cur_indent + 2) {
                    tokens_.push_back(Indent{});
                    cur_indent = indent;
                    indent = 0;
                }
            }
        }
        else if (c == '-' || c == '+' || c == '*' || c == '/' ||
                 c == ':' || c == '(' || c == ')' || c == ',' || c == '.') {

            tokens_.push_back(Char{c});
        }
        else if (c == '=' || c == '!' || c == '<' || c == '>' ) {
            if (input.peek() == '=') {
                input.get();
                switch (c) {
                    case '!' :
                        tokens_.push_back(NotEq{});
                        break;
                    case '=' :
                        tokens_.push_back(Eq{});
                        break;
                    case '<' :
                        tokens_.push_back(LessOrEq{});
                        break;
                    case '>' :
                        tokens_.push_back(GreaterOrEq{});
                        break;
                }
            }
            else {
                tokens_.push_back(Char{c});
            }
        }
        else if (c == '\n') {
            if ((tokens_.size() != 0) && !(tokens_.back().Is<Newline>())) {
                tokens_.push_back(Newline());
            }
        }
        else if (c == '#') {
            while (true) {
                if (!input) {
                    break;
                }
                if (input.peek() == '\n') {
                    break;
                }
                input.get();
            }
        }
    }

    if ((tokens_.size() != 0) && !(tokens_.back().Is<Newline>()) && !(tokens_.back().Is<Dedent>())) {
        tokens_.push_back(Newline());
    }

    tokens_.push_back(Eof());
}

const Token& Lexer::CurrentToken() const {
    return tokens_[token_id_];
}

Token Lexer::NextToken() {
    if (token_id_ < (tokens_.size() - 1)) {
        ++token_id_;
    }
    return tokens_[token_id_];
}

Token Lexer::LoadNumber(std::istream& input) {
    using namespace std::literals;
    using namespace token_type;

    std::string parsed_num;

    auto read_char = [&parsed_num, &input] {
        parsed_num += static_cast<char>(input.get());
        if (!input) {
            throw LexerError("Failed to read number from stream"s);
        }
    };

  // Считывает одну или более цифр в parsed_num из input
    auto read_digits = [&input, read_char] {
        if (!std::isdigit(input.peek())) {
            throw LexerError("A digit is expected"s);
        }
        while (std::isdigit(input.peek())) {
            read_char();
        }
    };

    read_digits();

    try {
        return Number{std::stoi(parsed_num)};
    } catch (...) {
        throw LexerError("Failed to convert "s + parsed_num + " to number"s);
    }
}

Token Lexer::LoadId(std::istream& input) {
    using namespace std::literals;

    auto it = std::istreambuf_iterator<char>(input);
    auto end = std::istreambuf_iterator<char>();

    std::string s;
    while (true) {
        if (it ==  end) {
            break;
        }
        const char ch = *it;
        if (ch !=  '_' && !(ch >= 'A' && ch <= 'Z') &&  !(ch >= 'a' && ch <= 'z') && !(ch >= '0' && ch <= '9')) {
            break;
        }
        else {
            s.push_back(ch);
        }
        std::next(it);//++it;
    }

    if (s == "class"s) {
        return token_type::Class{};
    }
    else if (s == "return"s) {
        return token_type::Return{};
    }
    else if (s == "if"s) {
        return token_type::If{};
    }
    else if (s == "else"s) {
        return token_type::Else{};
    }
    else if (s == "def"s) {
        return token_type::Def{};;
    }
    else if (s == "print"s) {
        return token_type::Print{};
    }
    else if (s == "or"s) {
        return token_type::Or{};
    }
    else if (s == "None"s) {
        return token_type::None{};
    }
    else if (s == "and"s) {
        return token_type::And{};
    }
    else if (s == "not"s) {
        return token_type::Not{};
    }
    else if (s == "True"s) {
        return token_type::True{};
    }
    else if (s == "False"s) {
        return token_type::False{};
    }
    else {
        return token_type::Id{s};
    }
}

Token Lexer::LoadString(std::istream& input,  char c) {
    using namespace std::literals;

    auto it = std::istreambuf_iterator<char>(input);
    auto end = std::istreambuf_iterator<char>();
    std::string s;

    while (true) {
        if (it == end) {
            throw LexerError("String parsing error");
        }

        const char ch = *it;
        if (ch ==  c) {
            std::next(it);//++it;
            break;
        }
        else if (ch == '\'' || ch == '\"') {
            s.push_back(ch);
        }
        else if (ch == '\\') {
            std::next(it);//++it;

            if (it == end) {
                throw LexerError("String parsing error");
            }

            const char escaped_char = *(it);
            // Обрабатываем одну из последовательностей: \\, \n, \t, \r, \"
            switch (escaped_char) {
                case 'n':
                    s.push_back('\n');
                    break;
                case 't':
                    s.push_back('\t');
                    break;
                case 'r':
                    s.push_back('\r');
                    break;
                case '"':
                    s.push_back('"');
                    break;
                case '\'':
                    s.push_back('\'');
                    break;
                case '\\':
                    s.push_back('\\');
                    break;
                default:
                // Встретили неизвестную escape-последовательность
                throw LexerError("Unrecognized escape sequence \\"s + escaped_char);
            }
        }
        else {
            s.push_back(ch);
        }
        std::next(it);//++it;
    }

    return token_type::String{s};
}

}  // namespace parse
