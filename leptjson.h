#ifndef LEPTJSON_LEPTJSON_H
#define LEPTJSON_LEPTJSON_H

typedef enum {
    LEPT_NULL, LEPT_FALSE, LEPT_TRUE, LEPT_NUMBER, LEPT_STRING, LEPT_ARRAY, LEPT_OBJECT
} lept_type;

typedef struct {
    lept_type type;
} lept_value;

enum {
    LEPT_PARSE_OK = 0,
    LEPT_PARSE_EXPECT_VALUE, // only whitespaces
    LEPT_PARSE_INVALID_VALUE,
    LEPT_PARSE_ROOT_NOT_SINGULAR // non-whitespace occurs after a value is parsed
};

/* Parse string json into v. If fail, set v to LEPT_NULL */
int lept_parse(lept_value *v, const char *json);

lept_type lept_get_type(const lept_value *v);

#endif //LEPTJSON_LEPTJSON_H
