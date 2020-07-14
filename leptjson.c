#ifdef _WINDOWS
#define _CRTDBG_MAP_ALLOC
#include <crtdbg.h> /* Windows 下检测内存泄漏，
* Linux 下可使用 Valgrind：valgrind --leak-check=full ./leptjson_test  */
#endif


#include "leptjson.h"
#include <assert.h>  /* assert() */
#include <stdlib.h>  /* NULL, malloc(), realloc(), free() */
#include <stdio.h>
#include <string.h>  /* memcpy() */
#include <errno.h>   /* errno, ERANGE */
#include <math.h>

#ifndef LEPT_PARSE_STACK_INIT_SIZE
#define LEPT_PARSE_STACK_INIT_SIZE 256
#endif

/* 字符入栈 */
#define PUTC(c, ch) do { *(char*) lept_context_push(c, sizeof(char)) = (ch); } while(0)

/* 判断 c 数组的第一个元素是否 ch，并返回下一个指针 */
#define EXPECT(c, ch)\
    do {\
        assert(*c->json == (ch));\
        c->json++;\
    } while(0)

#define ISDIGIT(ch) ((ch) >= '0' && (ch) <= '9')

#define ISDIGIT1TO9(ch) ((ch) >= '1' && (ch) <= '9')

#define STRING_ERROR(ret) do { c->top = head; return ret; } while(0)

/**
 * 函数间传递参数：减少解析函数之间传递多个参数
 */
typedef struct {
    const char *json;
    /*
     * 缓冲区：解析字符串/对象/数组时把结果存储在临时缓冲区，再用 lept_set_xxx 完成设值
     * 动态扩展：完成解析之前，缓冲区的大小不能预知，因此采用动态数组结构（用索引，不要用指针）
     * 堆栈：避免每次解析都重新创建一个动态数组，因此可以重用，并保持先进后出的方式访问
     */
    char *stack;
    size_t size, top;
} lept_context;

static int lept_parse_value(lept_context *c, lept_value *v);

/**
 * 堆栈压入
 *
 * @param c
 * @param size
 * @return
 */
static void *lept_context_push(lept_context *c, size_t size) {
    void *ret;
    assert(size > 0);
    /* 堆栈空间大于入栈数据 */
    if (c->top + size >= c->size) {
        if (c->size == 0) {
            c->size = LEPT_PARSE_STACK_INIT_SIZE;
        }
        /* 如果栈空间不足，则动态扩展 1.5 倍：https://www.zhihu.com/question/25079705/answer/30030883 */
        while (c->top + size >= c->size) {
            c->size += c->size >> 1;
        }
        /* 重新分配内存，为 c->stack 内存块重新分配 c->size 字节的内存，函数返回一个指向它的指针
         * 其中 realloc(NULL, size) <=> malloc(size) */
        c->stack = (char *) realloc(c->stack, c->size);
    }
    ret = c->stack + c->top;
    c->top += size;
    return ret;
}

/**
 * 堆栈弹出
 *
 * @param c
 * @param size
 * @return 返回 void * 指针可指向任何类型数据
 */
static void *lept_context_pop(lept_context *c, size_t size) {
    assert(c->top >= size);
    return c->stack + (c->top -= size);
}

/**
 * 去除有效文本前面所有的空白字符
 *
 * @param c
 */
static void lept_parse_whitespace(lept_context *c) {
    const char *p = c->json;
    while (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r') {
        p++;
    }
    c->json = p;
}

/**
 * 解析 null
 *
 * @param c
 * @param v
 * @param judge_arr
 * @return
 */
static int lept_parse_null(lept_context *c, lept_value *v) {
    EXPECT(c, 'n');
    const char *p = "ull";
    while (*p != '\0') {
        if (c->json[0] != *p) {
            return LEPT_PARSE_INVALID_VALUE;
        }
        c->json++;
        p++;
    }
    v->type = LEPT_NULL;
    return LEPT_PARSE_OK;
}

/**
 * 解析 null、true、false
 *
 * @param c
 * @param v
 * @param literal
 * @param type
 * @return
 */
static int lept_parse_literal(lept_context *c, lept_value *v, const char *literal, lept_type type) {

    /* C 语言中表示数组长度、索引值 */
    size_t i;
    EXPECT(c, literal[0]);
    for (i = 0; literal[i + 1]; i++)
        if (c->json[i] != literal[i + 1])
            return LEPT_PARSE_INVALID_VALUE;
    c->json += i;
    v->type = type;
    return LEPT_PARSE_OK;
}


/**
 * 解析 number
 *
 * @param c
 * @param v
 * @return
 */
static int lept_parse_number(lept_context *c, lept_value *v) {
    const char *p = c->json;

    /* 负号 */
    if (*p == '-') {
        p++;
    }

    /* 整数 */
    if (*p == '0') {
        p++;
    } else {
        if (!ISDIGIT1TO9(*p)) {
            return LEPT_PARSE_INVALID_VALUE;
        }
        for (p++; ISDIGIT(*p); p++);
    }

    /* 小数 */
    if (*p == '.') {
        p++;
        if (!ISDIGIT(*p)) {
            return LEPT_PARSE_INVALID_VALUE;
        }
        for (p++; ISDIGIT(*p); p++);
    }

    /* 指数 */
    if (*p == 'e' || *p == 'E') {
        p++;
        if (*p == '+' || *p == '-') {
            p++;
        }
        if (!ISDIGIT(*p)) {
            return LEPT_PARSE_INVALID_VALUE;
        }
        for (p++; ISDIGIT(*p); p++);
    }

    /* 转换成二进制，并处理过大的值 */
    errno = 0;
    v->u.n = strtod(c->json, NULL);
    if (errno == ERANGE && (v->u.n == HUGE_VAL || v->u.n == -HUGE_VAL)) {
        return LEPT_PARSE_NUMBER_TOO_BIG;
    }
    v->type = LEPT_NUMBER;
    c->json = p;
    return LEPT_PARSE_OK;
}

/**
 * 解析 4 位十六进制数字
 *
 * @param p
 * @param u
 * @return
 */
static const char *lept_parse_hex4(const char *p, unsigned *u) {
    int i;
    *u = 0;
    for (i = 0; i < 4; i++) {
        char ch = *p++;
        *u <<= 4;
        if (ch >= '0' && ch <= '9') {
            *u |= ch - '0';
        } else if (ch >= 'A' && ch <= 'F') {
            *u |= ch - ('A' - 10);
        } else if (ch >= 'a' && ch <= 'f') {
            *u |= ch - ('a' - 10);
        } else {
            return NULL;
        }
    }
    return p;
}

/**
 * UTF-8 编码
 *
 * @param c
 * @param u
 */
static void lept_encode_utf8(lept_context *c, unsigned u) {
    if (u <= 0x7F)
        PUTC(c, u & 0xFF);
    else if (u <= 0x7FF) {
        PUTC(c, 0xC0 | ((u >> 6) & 0xFF));
        PUTC(c, 0x80 | (u & 0x3F));
    } else if (u <= 0xFFFF) {
        PUTC(c, 0xE0 | ((u >> 12) & 0xFF));
        PUTC(c, 0x80 | ((u >> 6) & 0x3F));
        PUTC(c, 0x80 | (u & 0x3F));
    } else {
        assert(u <= 0x10FFFF);
        PUTC(c, 0xF0 | ((u >> 18) & 0xFF));
        PUTC(c, 0x80 | ((u >> 12) & 0x3F));
        PUTC(c, 0x80 | ((u >> 6) & 0x3F));
        PUTC(c, 0x80 | (u & 0x3F));
    }
}

/**
 * 解析 string
 *
 * @param c
 * @param v
 * @return
 */
static int lept_parse_string(lept_context *c, lept_value *v) {
    size_t head = c->top, len;
    unsigned u, u2;
    const char *p;
    EXPECT(c, '\"');
    p = c->json;
    for (;;) {
        char ch = *p++;
        switch (ch) {
            /* 找到末尾的引号 */
            case '\"':
                len = c->top - head;
                lept_set_string(v, (const char *) lept_context_pop(c, len), len);
                c->json = p;
                return LEPT_PARSE_OK;
                /* 找到反斜杠，添加转义字符 */
            case '\\':
                switch (*p++) {
                    case '\"':
                        PUTC(c, '\"');
                        break;
                    case '\\':
                        PUTC(c, '\\');
                        break;
                    case '/':
                        PUTC(c, '/');
                        break;
                    case 'b':
                        PUTC(c, '\b');
                        break;
                    case 'f':
                        PUTC(c, '\f');
                        break;
                    case 'n':
                        PUTC(c, '\n');
                        break;
                    case 'r':
                        PUTC(c, '\r');
                        break;
                    case 't':
                        PUTC(c, '\t');
                        break;
                    case 'u':
                        if (!(p = lept_parse_hex4(p, &u))) {
                            STRING_ERROR(LEPT_PARSE_INVALID_UNICODE_HEX);
                        }
                        if (u >= 0xD800 && u <= 0xDBFF) { /* surrogate pair */
                            if (*p++ != '\\') {
                                STRING_ERROR(LEPT_PARSE_INVALID_UNICODE_SURROGATE);
                            }
                            if (*p++ != 'u') {
                                STRING_ERROR(LEPT_PARSE_INVALID_UNICODE_SURROGATE);
                            }
                            if (!(p = lept_parse_hex4(p, &u2))) {
                                STRING_ERROR(LEPT_PARSE_INVALID_UNICODE_HEX);
                            }

                            if (u2 < 0xDC00 || u2 > 0xDFFF) {
                                STRING_ERROR(LEPT_PARSE_INVALID_UNICODE_SURROGATE);
                            }
                            u = (((u - 0xD800) << 10) | (u2 - 0xDC00)) + 0x10000;
                        }
                        lept_encode_utf8(c, u);
                        break;
                    default:
                        STRING_ERROR(LEPT_PARSE_INVALID_STRING_ESCAPE);
                }
                break;
            case '\0':
                STRING_ERROR(LEPT_PARSE_MISS_QUOTATION_MARK);
                /* 不合法的字符范围：%x00 ~ %x1F */
            default:
                if ((unsigned char) ch < 0x20) {
                    STRING_ERROR(LEPT_PARSE_INVALID_STRING_CHAR);
                }
                PUTC(c, ch);
        }
    }
}


static int lept_parse_array(lept_context *c, lept_value *v) {
    size_t i, size = 0;
    int ret;
    EXPECT(c, '[');
    lept_parse_whitespace(c);

    /* 空数组 */
    if (*c->json == ']') {
        c->json++;
        v->type = LEPT_ARRAY;
        v->u.a.size = 0;
        v->u.a.e = NULL;
        return LEPT_PARSE_OK;
    }
    for (;;) {
        lept_value e;
        lept_init(&e);
        /* 解析当前元素，解析失败则退出循环 */
        if ((ret = lept_parse_value(c, &e)) != LEPT_PARSE_OK) {
            break;;
        }
        /* 为 c 分配 sizeof(lept_value) 个字节的内存*/
        void *mem = lept_context_push(c, sizeof(lept_value));
        /* 把 &e 的 sizeof(lept_value) 个字节复制到 lept_context_push(c, sizeof(lept_value)) */
        memcpy(mem, &e, sizeof(lept_value));
        size++;
        lept_parse_whitespace(c);

        /* 解析一个元素后遇到逗号，跳过 */
        if (*c->json == ',') {
            c->json++;
            lept_parse_whitespace(c);
        } else if (*c->json == ']') {
            c->json++;
            v->type = LEPT_ARRAY;
            v->u.a.size = size;
            size *= sizeof(lept_value);
            memcpy(v->u.a.e = (lept_value *) malloc(size), lept_context_pop(c, size), size);
            return LEPT_PARSE_OK;
        } else {
            ret = LEPT_PARSE_MISS_COMMA_OR_SQUARE_BRACKET;
            break;
        }
    }
    for (i = 0; i < size; i++) {
        lept_free((lept_value *) lept_context_pop(c, sizeof(lept_value)));
    }
    return ret;
}

/**
 * 解析 JSON 值
 *
 * @param c
 * @param v
 * @return
 */
static int lept_parse_value(lept_context *c, lept_value *v) {
    /* 根据首字符选择判断分支 */
    switch (*c->json) {
        case 'n':
            return lept_parse_literal(c, v, "null", LEPT_NULL);
        case 't':
            return lept_parse_literal(c, v, "true", LEPT_TRUE);
        case 'f':
            return lept_parse_literal(c, v, "false", LEPT_FALSE);
        case '[':
            return lept_parse_array(c, v);
        case '"':
            return lept_parse_string(c, v);
        case '\0':
            return LEPT_PARSE_EXPECT_VALUE;
        default:
            return lept_parse_number(c, v);
    }
}

/**
 * 解析 JSON
 *
 * @param v
 * @param json
 * @return
 */
int lept_parse(lept_value *v, const char *json) {
    assert(v != NULL);

    lept_context c;
    int ret;
    c.json = json;
    c.stack = NULL;
    c.size = c.top = 0;
    lept_init(v);

    /* 去除空白、换行符、制表符 */
    lept_parse_whitespace(&c);

    /**
     * 解析结果并判断结果
     * JSON 文本应该有 3 部分：JSON-text = ws value ws；
     * 以下判断第三部分，即解析空白然后检查 JSON 文本是否完结
     */
    if ((ret = lept_parse_value(&c, v)) == LEPT_PARSE_OK) {
        /* 解析成功后，再跳过后面的空白，判断是否已到末尾 */
        lept_parse_whitespace(&c);
        if (*c.json != '\0') {
            v->type = LEPT_NULL;
            ret = LEPT_PARSE_ROOT_NOT_SINGULAR;
        }
    }
    /* 最后确保所有数据从缓冲区弹出 */
    assert(c.top == 0);
    free(c.stack);
    return ret;
}

/**
 * 获取 JSON 类型（包括 null、true、false）
 *
 * @param v
 * @return
 */
lept_type lept_get_type(const lept_value *v) {
    assert(v != NULL);
    return v->type;
}

/**
 * 获取 JSON 值 boolean
 *
 * @param v
 * @return
 */
int lept_get_boolean(const lept_value *v) {
    assert(v != NULL && (v->type == LEPT_TRUE || v->type == LEPT_FALSE));
    return v->type == LEPT_TRUE;
}

/**
 * 设置 JSON 值 boolean
 *
 * @param v
 * @param b
 */
void lept_set_boolean(lept_value *v, int b) {
    lept_free(v);
    v->type = b ? LEPT_TRUE : LEPT_FALSE;
}

/**
 * 获取 JSON 值 number
 *
 * @param v
 * @return
 */
double lept_get_number(const lept_value *v) {
    assert(v != NULL && v->type == LEPT_NUMBER);
    return v->u.n;
}

/**
 * 设置 JSON 值 number
 *
 * @param v
 * @param n
 */
void lept_set_number(lept_value *v, double n) {
    lept_free(v);
    v->u.n = n;
    v->type = LEPT_NUMBER;
}

/**
 * 释放内存
 *
 * @param v
 */
void lept_free(lept_value *v) {
    assert(v != NULL);
    if (v->type == LEPT_STRING) {
        free(v->u.s.s);
    }
    /* 置空，避免重复释放 */
    v->type = LEPT_NULL;
}

/**
 * 获取 JSON 值 string
 *
 * @param v
 * @return
 */
const char *lept_get_string(const lept_value *v) {
    assert(v != NULL && v->type == LEPT_STRING);
    return v->u.s.s;
}

/**
 * 获取 JSON 值 string 长度
 *
 * @param v
 * @return
 */
size_t lept_get_string_length(const lept_value *v) {
    assert(v != NULL && v->type == LEPT_STRING);
    return v->u.s.len;
}

/**
 * 设置 JSON 值 string
 *
 * @param v
 * @param s
 * @param len
 */
void lept_set_string(lept_value *v, const char *s, size_t len) {
    /* 非空指针或长度为 0 的字符串 */
    assert(v != NULL && (s != NULL || len == 0));
    lept_free(v);
    /* 为字符串分配内存，把内容从 s 复制到 v->u.s.s，最后一位置为 '\0' */
    v->u.s.s = (char *) malloc(len + 1);
    memcpy(v->u.s.s, s, len);
    v->u.s.s[len] = '\0';
    v->u.s.len = len;
    v->type = LEPT_STRING;
}

/**
 * 获取 JSON 值 array 长度
 *
 * @param v
 * @return
 */
size_t lept_get_array_size(const lept_value *v) {
    assert(v != NULL && v->type == LEPT_ARRAY);
    return v->u.a.size;
}

/**
 * 获取 JSON 值 array 元素
 *
 * @param v
 * @param index
 * @return
 */
lept_value *lept_get_array_element(const lept_value *v, size_t index) {
    assert(v != NULL && v->type == LEPT_ARRAY);
    assert(index < v->u.a.size);
    return &v->u.a.e[index];
}
