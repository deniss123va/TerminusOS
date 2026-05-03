#include "cmd_calc.h"
#include "../lib/screen.h"
#include "../lib/string.h"

// ─── Рекурсивный парсер выражений ────────────────────────────────────────────
// Поддерживает: + - * / % ( ) унарный минус, целая арифметика

static const char* calc_src;
static bool calc_error;

static void skip_spaces() {
    while (*calc_src == ' ' || *calc_src == '\t') calc_src++;
}

static long parse_expr();   // forward

static long parse_number() {
    skip_spaces();
    if (*calc_src == '(') {
        calc_src++;             // skip '('
        long val = parse_expr();
        skip_spaces();
        if (*calc_src == ')') calc_src++;
        else calc_error = true;
        return val;
    }
    // унарный минус
    int sign = 1;
    if (*calc_src == '-') { sign = -1; calc_src++; }
    else if (*calc_src == '+') { calc_src++; }

    skip_spaces();
    if (*calc_src < '0' || *calc_src > '9') {
        calc_error = true;
        return 0;
    }
    long n = 0;
    while (*calc_src >= '0' && *calc_src <= '9')
        n = n * 10 + (*calc_src++ - '0');
    return sign * n;
}

static long parse_term() {
    long left = parse_number();
    while (true) {
        skip_spaces();
        char op = *calc_src;
        if (op != '*' && op != '/' && op != '%') break;
        calc_src++;
        long right = parse_number();
        if (op == '*') left *= right;
        else if (right == 0) { calc_error = true; return 0; }
        else if (op == '/') left /= right;
        else                left %= right;
    }
    return left;
}

static long parse_expr() {
    long left = parse_term();
    while (true) {
        skip_spaces();
        char op = *calc_src;
        if (op != '+' && op != '-') break;
        calc_src++;
        long right = parse_term();
        if (op == '+') left += right;
        else           left -= right;
    }
    return left;
}

// ─── Вывод числа без printf ───────────────────────────────────────────────────

static void print_long(long n) {
    if (n < 0) { print_char('-'); n = -n; }
    char buf[22];
    int i = 0;
    if (n == 0) { print_char('0'); return; }
    while (n > 0) { buf[i++] = '0' + (n % 10); n /= 10; }
    for (int j = i - 1; j >= 0; j--) print_char(buf[j]);
}

// ─── Точка входа ──────────────────────────────────────────────────────────────

void cmd_calc(const char* expr) {
    if (!expr || !*expr) {
        println("Usage: calc <expression>");
        println("  Examples: calc 2+2   calc 10*(3+4)   calc 100/7");
        return;
    }
    calc_src   = expr;
    calc_error = false;
    long result = parse_expr();
    skip_spaces();
    if (calc_error || *calc_src != '\0') {
        println("calc: invalid expression");
        return;
    }
    print("= ");
    print_long(result);
    print_char('\n');
}