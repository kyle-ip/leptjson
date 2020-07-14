#ifndef LEPTJSON_H__
#define LEPTJSON_H__

#include <stddef.h> /* size_t */

/* 项目名称_目录_文件名称_H__ */
/* 项目名称_H__ */

/* 通常枚举值用全大写，类型及函数则用小写 */
/* JSON 数据类型枚举 */
typedef enum {
    LEPT_NULL,
    LEPT_FALSE,
    LEPT_TRUE,
    LEPT_NUMBER,
    LEPT_STRING,
    LEPT_ARRAY,
    LEPT_OBJECT
} lept_type;


/* JSON 解析结果 */
enum {
    /* 解析成功 */
    LEPT_PARSE_OK = 0,

    /* 一个 JSON 只含有空白 */
    LEPT_PARSE_EXPECT_VALUE,

    /* 一个值之后，在空白之后还有其他字符 */
    LEPT_PARSE_INVALID_VALUE,

    /* 过大的数值 */
    LEPT_PARSE_NUMBER_TOO_BIG,

    /* 缺失引号 */
    LEPT_PARSE_MISS_QUOTATION_MARK,

    /* 其他字面值 */
    LEPT_PARSE_ROOT_NOT_SINGULAR,

    /* 不合法的转义字符 */
    LEPT_PARSE_INVALID_STRING_ESCAPE,

    /* 不合法的 unicode 十六进制数字 */
    LEPT_PARSE_INVALID_UNICODE_HEX,

    LEPT_PARSE_INVALID_UNICODE_SURROGATE,

    /* 不合法的字符串 */
    LEPT_PARSE_INVALID_STRING_CHAR,

    /* 缺失逗号或（右）中括号 */
    LEPT_PARSE_MISS_COMMA_OR_SQUARE_BRACKET
};



/* JSON 结构体 */
typedef struct lept_value lept_value;
struct lept_value {
    /* 值：使用共用体节省内存 */
    union {

        /* array：用到自身类型的指针，必须向前声明 */
        struct {
            lept_value *e;
            size_t size;
        } a;

        /* string：长度不固定，需要动态分配内存 */
        struct {
            char *s;
            size_t len;
        } s;
        /* number */
        double n;
    } u;

    /* 类型 */
    lept_type type;
} ;

/**
 * （调用访问函数前）对 JSON 对象类型初始化
 * do { ... } while(0) 把表达式转为语句，模仿无返回值的函数
 */
#define lept_init(v) do { (v)->type = LEPT_NULL; } while(0)

#define lept_set_null(v) lept_free(v);

/**
 * 解析 JSON，一般用法：
 *     lept_value v;
 *     const char json[] = ...;
 *     int ret = lept_parse(&v, json);
 * @param v     根节点指针
 * @param json  JSON 字符串
 * @return
 */
int lept_parse(lept_value *v, const char *json);

/**
 * 获取 JSON 类型（包括 null、true、false）
 *
 * @param v
 * @return
 */
lept_type lept_get_type(const lept_value *v);

/**
 * 获取 JSON 值 boolean
 *
 * @param v
 * @return
 */
int lept_get_boolean(const lept_value *v);

/**
 * 设置 JSON 值 boolean
 *
 * @param v
 * @param b
 */
void lept_set_boolean(lept_value *v, int b);

/**
 * 获取 JSON 值 number
 *
 * @param v
 * @return
 */
double lept_get_number(const lept_value *v);

/**
 * 设置 JSON 值 number
 *
 * @param v
 * @param n
 */
void lept_set_number(lept_value *v, double n);

/**
 * 释放内存
 *
 * @param v
 */
void lept_free(lept_value *v);

/**
 * 获取 JSON 值 string
 *
 * @param v
 * @return
 */
const char *lept_get_string(const lept_value *v);

/**
 * 获取 JSON 值 string 长度
 *
 * @param v
 * @return
 */
size_t lept_get_string_length(const lept_value *v);

/**
 * 设置 JSON 值 string
 *
 * @param v
 * @param s
 * @param len
 */
void lept_set_string(lept_value *v, const char *s, size_t len);

/**
 * 获取 JSON 值 array 长度
 *
 * @param v
 * @return
 */
size_t lept_get_array_size(const lept_value *v);

/**
 * 获取 JSON 值 array 元素
 *
 * @param v
 * @param index
 * @return
 */
lept_value * lept_get_array_element(const lept_value* v, size_t index);

/* LEPTJSON_H__ */
#endif
