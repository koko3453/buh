/*
 * jsmn.h - minimalistic JSON parser in C.
 * https://github.com/zserge/jsmn
 * MIT License.
 */
#ifndef JSMN_H
#define JSMN_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
  JSMN_UNDEFINED = 0,
  JSMN_OBJECT = 1,
  JSMN_ARRAY = 2,
  JSMN_STRING = 3,
  JSMN_PRIMITIVE = 4
} jsmntype_t;

typedef struct {
  jsmntype_t type;
  int start;
  int end;
  int size;
#ifdef JSMN_PARENT_LINKS
  int parent;
#endif
} jsmntok_t;

typedef struct {
  unsigned int pos;
  unsigned int toknext;
  int toksuper;
} jsmn_parser;

static void jsmn_init(jsmn_parser *parser) {
  parser->pos = 0;
  parser->toknext = 0;
  parser->toksuper = -1;
}

static jsmntok_t *jsmn_alloc_token(jsmn_parser *parser, jsmntok_t *tokens, size_t num_tokens) {
  if (parser->toknext >= num_tokens) return NULL;
  jsmntok_t *tok = &tokens[parser->toknext++];
  tok->start = tok->end = -1;
  tok->size = 0;
#ifdef JSMN_PARENT_LINKS
  tok->parent = -1;
#endif
  return tok;
}

static void jsmn_fill_token(jsmntok_t *token, jsmntype_t type, int start, int end) {
  token->type = type;
  token->start = start;
  token->end = end;
  token->size = 0;
}

static int jsmn_parse_primitive(jsmn_parser *parser, const char *js, size_t len, jsmntok_t *tokens, size_t num_tokens) {
  int start = (int)parser->pos;
  for (; parser->pos < len; parser->pos++) {
    switch (js[parser->pos]) {
      case '\t': case '\r': case '\n': case ' ': case ',': case ']': case '}':
        goto found;
      default:
        break;
    }
    if (js[parser->pos] < 32 || js[parser->pos] == 127) {
      return -1;
    }
  }
found: {
    jsmntok_t *tok = jsmn_alloc_token(parser, tokens, num_tokens);
    if (!tok) return -1;
    jsmn_fill_token(tok, JSMN_PRIMITIVE, start, (int)parser->pos);
#ifdef JSMN_PARENT_LINKS
    tok->parent = parser->toksuper;
#endif
    parser->pos--;
    return 0;
  }
}

static int jsmn_parse_string(jsmn_parser *parser, const char *js, size_t len, jsmntok_t *tokens, size_t num_tokens) {
  int start = (int)parser->pos;
  parser->pos++;
  for (; parser->pos < len; parser->pos++) {
    char c = js[parser->pos];
    if (c == '"') {
      jsmntok_t *tok = jsmn_alloc_token(parser, tokens, num_tokens);
      if (!tok) return -1;
      jsmn_fill_token(tok, JSMN_STRING, start + 1, (int)parser->pos);
#ifdef JSMN_PARENT_LINKS
      tok->parent = parser->toksuper;
#endif
      return 0;
    }
    if (c == '\\') {
      parser->pos++;
      if (parser->pos >= len) return -1;
    }
  }
  return -1;
}

static int jsmn_parse(jsmn_parser *parser, const char *js, size_t len, jsmntok_t *tokens, unsigned int num_tokens) {
  int r;
  int i;
  jsmntok_t *tok;

  for (; parser->pos < len; parser->pos++) {
    char c = js[parser->pos];
    switch (c) {
      case '{': case '[':
        tok = jsmn_alloc_token(parser, tokens, num_tokens);
        if (!tok) return -1;
        tok->type = (c == '{' ? JSMN_OBJECT : JSMN_ARRAY);
        tok->start = (int)parser->pos;
#ifdef JSMN_PARENT_LINKS
        tok->parent = parser->toksuper;
#endif
        if (parser->toksuper != -1) {
          tokens[parser->toksuper].size++;
        }
        parser->toksuper = (int)(parser->toknext - 1);
        break;
      case '}': case ']':
        for (i = (int)parser->toknext - 1; i >= 0; i--) {
          tok = &tokens[i];
          if (tok->start != -1 && tok->end == -1) {
            if ((tok->type == JSMN_OBJECT && c == '}') || (tok->type == JSMN_ARRAY && c == ']')) {
              tok->end = (int)parser->pos + 1;
              parser->toksuper = -1;
#ifdef JSMN_PARENT_LINKS
              parser->toksuper = tok->parent;
#else
              for (i = i - 1; i >= 0; i--) {
                if (tokens[i].start != -1 && tokens[i].end == -1) {
                  parser->toksuper = i;
                  break;
                }
              }
#endif
              break;
            } else {
              return -1;
            }
          }
        }
        break;
      case '"':
        r = jsmn_parse_string(parser, js, len, tokens, num_tokens);
        if (r < 0) return r;
        if (parser->toksuper != -1) tokens[parser->toksuper].size++;
        break;
      case '\t': case '\r': case '\n': case ' ': case ':': case ',':
        break;
      default:
        r = jsmn_parse_primitive(parser, js, len, tokens, num_tokens);
        if (r < 0) return r;
        if (parser->toksuper != -1) tokens[parser->toksuper].size++;
        break;
    }
  }

  for (i = (int)parser->toknext - 1; i >= 0; i--) {
    if (tokens[i].start != -1 && tokens[i].end == -1) return -1;
  }
  return (int)parser->toknext;
}

#ifdef __cplusplus
}
#endif

#endif
