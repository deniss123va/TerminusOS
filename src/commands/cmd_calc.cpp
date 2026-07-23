#include "cmd_calc.h"
#include "../lib/screen.h"
#include "../lib/string.h"

// ─── Рекурсивный парсер выражений ────────────────────────────────────────────
// Поддерживает:
//   Арифметика : + - * / % ** (степень)
//   Битовые    : & | ^ ~ << >>
//   Синтаксис  : ( )  унарный минус/плюс/тильда  0x... hex-литералы

static const char* calc_src;
static bool        calc_error;

static void skip_spaces() {
    while (*calc_src == ' ' || *calc_src == '\t') calc_src++;
}

// ─── Forward declarations ─────────────────────────────────────────────────────
static long parse_expr();

// ─── Число или подвыражение в скобках ────────────────────────────────────────
static long parse_number() {
    skip_spaces();

    // Скобки
    if (*calc_src == '(') {
        calc_src++;
        long val = parse_expr();
        skip_spaces();
        if (*calc_src == ')') calc_src++;
        else calc_error = true;
        return val;
    }

    // Унарный ~
    if (*calc_src == '~') {
        calc_src++;
        return ~parse_number();
    }

    // Унарный минус / плюс
    int sign = 1;
    if      (*calc_src == '-') { sign = -1; calc_src++; }
    else if (*calc_src == '+') {             calc_src++; }

    skip_spaces();

    // Hex литерал: 0x...
    if (*calc_src == '0' && (*(calc_src+1) == 'x' || *(calc_src+1) == 'X')) {
        calc_src += 2;
        if (!( (*calc_src >= '0' && *calc_src <= '9') ||
               (*calc_src >= 'a' && *calc_src <= 'f') ||
               (*calc_src >= 'A' && *calc_src <= 'F') )) {
            calc_error = true; return 0;
        }
        long n = 0;
        while (true) {
            char c = *calc_src;
            if      (c >= '0' && c <= '9') n = n * 16 + (c - '0');
            else if (c >= 'a' && c <= 'f') n = n * 16 + (c - 'a' + 10);
            else if (c >= 'A' && c <= 'F') n = n * 16 + (c - 'A' + 10);
            else break;
            calc_src++;
        }
        return sign * n;
    }

    // Десятичное число
    if (*calc_src < '0' || *calc_src > '9') { calc_error = true; return 0; }
    long n = 0;
    while (*calc_src >= '0' && *calc_src <= '9')
        n = n * 10 + (*calc_src++ - '0');
    return sign * n;
}

// ─── Целочисленная степень ────────────────────────────────────────────────────
static long ipow(long base, long exp) {
    if (exp < 0)  return 0;   // целая арифметика
    if (exp == 0) return 1;
    long result = 1;
    while (exp-- > 0) result *= base;
    return result;
}

// ─── Уровень 1: ** (степень, правоассоциативный) ─────────────────────────────
static long parse_power() {
    long left = parse_number();
    skip_spaces();
    if (*calc_src == '*' && *(calc_src+1) == '*') {
        calc_src += 2;
        long exp = parse_power();   // правая ассоциативность
        left = ipow(left, exp);
    }
    return left;
}

// ─── Уровень 2: * / % ────────────────────────────────────────────────────────
static long parse_term() {
    long left = parse_power();
    while (true) {
        skip_spaces();
        char op = *calc_src;
        if (op == '*' && *(calc_src+1) == '*') break;  // не хватать **
        if (op != '*' && op != '/' && op != '%') break;
        calc_src++;
        long right = parse_power();
        if (op == '*') left *= right;
        else if (right == 0) { calc_error = true; return 0; }
        else if (op == '/') left /= right;
        else                left %= right;
    }
    return left;
}

// ─── Уровень 3: + - ──────────────────────────────────────────────────────────
static long parse_add() {
    long left = parse_term();
    while (true) {
        skip_spaces();
        char op = *calc_src;
        if (op != '+' && op != '-') break;
        calc_src++;
        long right = parse_term();
        left = (op == '+') ? left + right : left - right;
    }
    return left;
}

// ─── Уровень 4: << >> ────────────────────────────────────────────────────────
static long parse_shift() {
    long left = parse_add();
    while (true) {
        skip_spaces();
        if (*calc_src == '<' && *(calc_src+1) == '<') {
            calc_src += 2;
            long r = parse_add();
            left <<= r;
        } else if (*calc_src == '>' && *(calc_src+1) == '>') {
            calc_src += 2;
            long r = parse_add();
            left >>= r;
        } else break;
    }
    return left;
}

// ─── Уровень 5: & ────────────────────────────────────────────────────────────
static long parse_bitand() {
    long left = parse_shift();
    while (true) {
        skip_spaces();
        if (*calc_src == '&') { calc_src++; left &= parse_shift(); }
        else break;
    }
    return left;
}

// ─── Уровень 6: ^ (XOR) ──────────────────────────────────────────────────────
static long parse_bitxor() {
    long left = parse_bitand();
    while (true) {
        skip_spaces();
        if (*calc_src == '^') { calc_src++; left ^= parse_bitand(); }
        else break;
    }
    return left;
}

// ─── Уровень 7: | ────────────────────────────────────────────────────────────
static long parse_expr() {
    long left = parse_bitxor();
    while (true) {
        skip_spaces();
        if (*calc_src == '|') { calc_src++; left |= parse_bitxor(); }
        else break;
    }
    return left;
}

// ─── Вывод числа ─────────────────────────────────────────────────────────────
static void print_long(long n) {
    if (n < 0) { print_char('-'); n = -n; }
    if (n == 0) { print_char('0'); return; }
    char buf[22]; int i = 0;
    while (n > 0) { buf[i++] = '0' + (n % 10); n /= 10; }
    for (int j = i - 1; j >= 0; j--) print_char(buf[j]);
}

static void print_hex(long n) {
    const char* h = "0123456789ABCDEF";
    print("0x");
    // Ведущие нули убираем, но хотя бы 1 цифра
    unsigned long u = (unsigned long)n;
    bool leading = true;
    for (int shift = 60; shift >= 0; shift -= 4) {
        int nibble = (u >> shift) & 0xF;
        if (nibble || !leading || shift == 0) {
            print_char(h[nibble]);
            leading = false;
        }
    }
}

// ─── Точка входа ─────────────────────────────────────────────────────────────
void cmd_calc(const char* expr) {
    if (!expr || !*expr) {
        println("Usage: calc <expression>");
        println("  Arithmetic : + - * / % ** (power)");
        println("  Bitwise    : & | ^ ~ << >>");
        println("  Hex input  : 0xFF & 0b not supported, use 0x");
        println("  Examples   : calc 2**10   calc 0xFF & 0x0F   calc ~0 >> 1");
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
    if (result != 0) {
        print("  (");
        print_hex(result);
        print_char(')');
    }
    print_char('\n');
}