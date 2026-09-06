int isspace(char c) {
    return (c == '\t' || c == '\n' || c == '\v' || c == '\f' || c == '\r');
}

int isalpha(char c) {
    return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z');
}

int isdigit(char c) {
    return c >= '0' && c <= '9';
}

int isxdigit(char c) {
    return isdigit(c) || c == 'a' || 
             c == 'b' || c == 'c' || 
             c == 'd' || c == 'e' || 
             c == 'f' || c == 'A' || 
             c == 'B' || c == 'C' || 
             c == 'D' || c == 'E' || 
             c == 'F';
}

int isprint(char c) {
    return c > 0x1f && c != 0x7f;
}

int isalnum(char c) {
    return isalpha(c) || isdigit(c);
}

int ispunct(char c) {
    return isprint(c) && !isalnum(c) && c != ' ';
}

int isupper(char c) {
    return c >= 'A' && c <= 'Z';
}

int islower(char c) {
    return c >= 'a' && c <= 'z';
}

int toupper(char c) {
    return (!isalpha(c)) ? c :
           (isupper(c))  ? c :
           c - 0x20;
}

int tolower(char c) {
    return (!isalpha(c)) ? c :
           (islower(c))  ? c :
           c + 0x20;
}

int abs(int n) {
    return n < 0 ? -n : n;
}