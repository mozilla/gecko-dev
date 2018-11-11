/*

 JS Beautifier
---------------

  Written by Einars "elfz" Lielmanis, <elfz@laacz.lv>
      http://elfz.laacz.lv/beautify/

  Originally converted to javascript by Vital, <vital76@gmail.com>
      http://my.opera.com/Vital/blog/2007/11/21/javascript-beautify-on-javascript-translated


  You are free to use this in any way you want, in case you find this useful or working for you.

  Usage:
    js_beautify(js_source_text);


*/


function js_beautify(js_source_text, indent_size, indent_character)
{

    var input, output, token_text, last_type, current_mode, modes, indent_level, indent_string;
    var whitespace, wordchar, punct;

    indent_character = indent_character || ' ';
    indent_size = indent_size || 4;

    indent_string = '';
    while (indent_size--) indent_string += indent_character;

    input = js_source_text;

    last_word = ''; // last 'TK_WORD' passed
    last_type = 'TK_START_EXPR'; // last token type
    last_text = ''; // last token text
    output = '';

    whitespace = "\n\r\t ".split('');
    wordchar = 'abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_$'.split('');
    punct = '+ - * / % & ++ -- = += -= *= /= %= == === != !== > < >= <= >> << >>> >>>= >>= <<= && &= | || ! !! , : ? ^ ^= |='.split(' ');

    // words which should always start on new line.
    line_starters = 'continue,try,throw,return,var,if,switch,case,default,for,while,break,function'.split(',');

    // states showing if we are currently in expression (i.e. "if" case) - 'EXPRESSION', or in usual block (like, procedure), 'BLOCK'.
    // some formatting depends on that.
    current_mode = 'BLOCK';
    modes = [current_mode];

    indent_level = 0;
    parser_pos = 0; // parser position
    in_case = false; // flag for parser that case/default has been processed, and next colon needs special attention
    while (true) {
        var t = get_next_token(parser_pos);
        token_text = t[0];
        token_type = t[1];
        if (token_type == 'TK_EOF') {
            break;
        }

        switch (token_type) {

        case 'TK_START_EXPR':

            set_mode('EXPRESSION');
            if (last_type == 'TK_END_EXPR' || last_type == 'TK_START_EXPR') {
                // do nothing on (( and )( and ][ and ]( ..
                } else if (last_type != 'TK_WORD' && last_type != 'TK_OPERATOR') {
                print_space();
            } else if (in_array(last_word, line_starters) && last_word != 'function') {
                print_space();
            }
            print_token();
            break;

        case 'TK_END_EXPR':

            print_token();
            restore_mode();
            break;

        case 'TK_START_BLOCK':

            set_mode('BLOCK');
            if (last_type != 'TK_OPERATOR' && last_type != 'TK_START_EXPR') {
                if (last_type == 'TK_START_BLOCK') {
                    print_newline();
                } else {
                    print_space();
                }
            }
            print_token();
            indent();
            break;

        case 'TK_END_BLOCK':

            if (last_type == 'TK_END_EXPR') {
                unindent();
                print_newline();
            } else if (last_type == 'TK_END_BLOCK') {
                unindent();
                print_newline();
            } else if (last_type == 'TK_START_BLOCK') {
                // nothing
                unindent();
            } else {
                unindent();
                print_newline();
            }
            print_token();
            restore_mode();
            break;

        case 'TK_WORD':

            if (token_text == 'case' || token_text == 'default') {
                if (last_text == ':') {
                    // switch cases following one another
                    remove_indent();
                } else {
                    // case statement starts in the same line where switch
                    unindent();
                    print_newline();
                    indent();
                }
                print_token();
                in_case = true;
                break;
            }

            prefix = 'NONE';
            if (last_type == 'TK_END_BLOCK') {
                if (!in_array(token_text.toLowerCase(), ['else', 'catch', 'finally'])) {
                    prefix = 'NEWLINE';
                } else {
                    prefix = 'SPACE';
                    print_space();
                }
            } else if (last_type == 'TK_END_COMMAND' && current_mode == 'BLOCK') {
                prefix = 'NEWLINE';
            } else if (last_type == 'TK_END_COMMAND' && current_mode == 'EXPRESSION') {
                prefix = 'SPACE';
            } else if (last_type == 'TK_WORD') {
                prefix = 'SPACE';
            } else if (last_type == 'TK_START_BLOCK') {
                prefix = 'NEWLINE';
            } else if (last_type == 'TK_END_EXPR') {
                print_space();
                prefix = 'NEWLINE';
            }

            if (in_array(token_text, line_starters) || prefix == 'NEWLINE') {

                if (last_text == 'else') {
                    // no need to force newline on else break
                    print_space();
                } else if ((last_type == 'TK_START_EXPR' || last_text == '=') && token_text == 'function') {
                    // no need to force newline on 'function': (function
                    // DONOTHING
                    } else if (last_type == 'TK_WORD' && (last_text == 'return' || last_text == 'throw')) {
                    // no newline between 'return nnn'
                    print_space();
                } else
                if (last_type != 'TK_END_EXPR') {
                    if ((last_type != 'TK_START_EXPR' || token_text != 'var') && last_text != ':') {
                        // no need to force newline on 'var': for (var x = 0...)
                        if (token_text == 'if' && last_type == 'TK_WORD' && last_word == 'else') {
                            // no newline for } else if {
                            print_space();
                        } else {
                            print_newline();
                        }
                    }
                }
            } else if (prefix == 'SPACE') {
                print_space();
            }
            print_token();
            last_word = token_text;
            break;

        case 'TK_END_COMMAND':

            print_token();
            break;

        case 'TK_STRING':

            if (last_type == 'TK_START_BLOCK' || last_type == 'TK_END_BLOCK') {
                print_newline();
            } else if (last_type == 'TK_WORD') {
                print_space();
            }
            print_token();
            break;

        case 'TK_OPERATOR':
            start_delim = true;
            end_delim = true;

            if (token_text == ':' && in_case) {
                print_token(); // colon really asks for separate treatment
                print_newline();
                break;
            }

            in_case = false;

            if (token_text == ',') {
                if (last_type == 'TK_END_BLOCK') {
                    print_token();
                    print_newline();
                } else {
                    if (current_mode == 'BLOCK') {
                        print_token();
                        print_newline();
                    } else {
                        print_token();
                        print_space();
                    }
                }
                break;
            } else if (token_text == '--' || token_text == '++') { // unary operators special case
                if (last_text == ';') {
                    // space for (;; ++i)
                    start_delim = true;
                    end_delim = false;
                } else {
                    start_delim = false;
                    end_delim = false;
                }
            } else if (token_text == '!' && last_type == 'TK_START_EXPR') {
                // special case handling: if (!a)
                start_delim = false;
                end_delim = false;
            } else if (last_type == 'TK_OPERATOR') {
                start_delim = false;
                end_delim = false;
            } else if (last_type == 'TK_END_EXPR') {
                start_delim = true;
                end_delim = true;
            } else if (token_text == '.') {
                // decimal digits or object.property
                start_delim = false;
                end_delim = false;

            } else if (token_text == ':') {
                // zz: xx
                // can't differentiate ternary op, so for now it's a ? b: c; without space before colon
                start_delim = false;
            }
            if (start_delim) {
                print_space();
            }

            print_token();

            if (end_delim) {
                print_space();
            }
            break;

        case 'TK_BLOCK_COMMENT':

            print_newline();
            print_token();
            print_newline();
            break;

        case 'TK_COMMENT':

            // print_newline();
            print_space();
            print_token();
            print_newline();
            break;

        case 'TK_UNKNOWN':
            print_token();
            break;
        }

        if (token_type != 'TK_COMMENT') {
            last_type = token_type;
            last_text = token_text;
        }
    }

    return output;




    function print_newline(ignore_repeated)
    {
        ignore_repeated = typeof ignore_repeated == 'undefined' ? true: ignore_repeated;
        output = output.replace(/[ \t]+$/, ''); // remove possible indent
        if (output == '') return; // no newline on start of file
        if (output.substr(output.length - 1) != "\n" || !ignore_repeated) {
            output += "\n";
        }
        for (var i = 0; i < indent_level; i++) {
            output += indent_string;
        }
    }



    function print_space()
    {
        if (output && output.substr(output.length - 1) != ' ' && output.substr(output.length - 1) != '\n') { // prevent occassional duplicate space
            output += ' ';
        }
    }


    function print_token()
    {
        output += token_text;
    }

    function indent()
    {
        indent_level++;
    }


    function unindent()
    {
        if (indent_level) {
            indent_level--;
        }
    }


    function remove_indent()
    {
        if (output.substr(output.length - indent_string.length) == indent_string) {
            output = output.substr(0, output.length - indent_string.length);
        }
    }


    function set_mode(mode)
    {
        modes.push(current_mode);
        current_mode = mode;
    }


    function restore_mode()
    {
        current_mode = modes.pop();
    }



    function get_next_token()
    {
        var n_newlines = 0;
        var c = '';

        do {
            if (parser_pos >= input.length) {
                return ['', 'TK_EOF'];
            }
            c = input.charAt(parser_pos);

            parser_pos += 1;
            if (c == "\n") {
                n_newlines += 1;
            }
        }
        while (in_array(c, whitespace));

        if (n_newlines > 1) {
            for (var i = 0; i < n_newlines; i++) {
                print_newline(i == 0);
            }
        }
        var wanted_newline = n_newlines == 1;


        if (in_array(c, wordchar)) {
            if (parser_pos < input.length) {
                while (in_array(input.charAt(parser_pos), wordchar)) {
                    c += input.charAt(parser_pos);
                    parser_pos += 1;
                    if (parser_pos == input.length) break;
                }
            }

            // small and surprisingly unugly hack for 1E-10 representation
            if (parser_pos != input.length && c.match(/^[0-9]+[Ee]$/) && input.charAt(parser_pos) == '-') {
                parser_pos += 1;

                var t = get_next_token(parser_pos);
                next_word = t[0];
                next_type = t[1];

                c += '-' + next_word;
                return [c, 'TK_WORD'];
            }

            if (c == 'in') { // hack for 'in' operator
                return [c, 'TK_OPERATOR'];
            }
            return [c, 'TK_WORD'];
        }

        if (c == '(' || c == '[') {
            return [c, 'TK_START_EXPR'];
        }

        if (c == ')' || c == ']') {
            return [c, 'TK_END_EXPR'];
        }

        if (c == '{') {
            return [c, 'TK_START_BLOCK'];
        }

        if (c == '}') {
            return [c, 'TK_END_BLOCK'];
        }

        if (c == ';') {
            return [c, 'TK_END_COMMAND'];
        }

        if (c == '/') {
            // peek for comment /* ... */
            if (input.charAt(parser_pos) == '*') {
                comment = '';
                parser_pos += 1;
                if (parser_pos < input.length) {
                    while (! (input.charAt(parser_pos) == '*' && input.charAt(parser_pos + 1) && input.charAt(parser_pos + 1) == '/') && parser_pos < input.length) {
                        comment += input.charAt(parser_pos);
                        parser_pos += 1;
                        if (parser_pos >= input.length) break;
                    }
                }
                parser_pos += 2;
                return ['/*' + comment + '*/', 'TK_BLOCK_COMMENT'];
            }
            // peek for comment // ...
            if (input.charAt(parser_pos) == '/') {
                comment = c;
                while (input.charAt(parser_pos) != "\x0d" && input.charAt(parser_pos) != "\x0a") {
                    comment += input.charAt(parser_pos);
                    parser_pos += 1;
                    if (parser_pos >= input.length) break;
                }
                parser_pos += 1;
                if (wanted_newline) {
                    print_newline();
                }
                return [comment, 'TK_COMMENT'];
            }

        }

        if (c == "'" || // string
        c == '"' || // string
        (c == '/' &&
        ((last_type == 'TK_WORD' && last_text == 'return') || (last_type == 'TK_START_EXPR' || last_type == 'TK_END_BLOCK' || last_type == 'TK_OPERATOR' || last_type == 'TK_EOF' || last_type == 'TK_END_COMMAND')))) { // regexp
            sep = c;
            c = '';
            esc = false;

            if (parser_pos < input.length) {

                while (esc || input.charAt(parser_pos) != sep) {
                    c += input.charAt(parser_pos);
                    if (!esc) {
                        esc = input.charAt(parser_pos) == '\\';
                    } else {
                        esc = false;
                    }
                    parser_pos += 1;
                    if (parser_pos >= input.length) break;
                }

            }

            parser_pos += 1;
            if (last_type == 'TK_END_COMMAND') {
                print_newline();
            }
            return [sep + c + sep, 'TK_STRING'];
        }

        if (in_array(c, punct)) {
            while (parser_pos < input.length && in_array(c + input.charAt(parser_pos), punct)) {
                c += input.charAt(parser_pos);
                parser_pos += 1;
                if (parser_pos >= input.length) break;
            }
            return [c, 'TK_OPERATOR'];
        }

        return [c, 'TK_UNKNOWN'];
    }


    function in_array(what, arr)
    {
        for (var i = 0; i < arr.length; i++)
        {
            if (arr[i] == what) return true;
        }
        return false;
    }
}
