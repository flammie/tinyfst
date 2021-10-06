//! @file tinyfst.c
//! An experiment on minimal FST data structure...

// XXX: before config.h
#if HAVE_CONFIG_H
#  include "config.h"
#else
#  define _POSIX_C_SOURCE 200809L
#endif

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>

#ifndef INFINITY
#  define INFINITY (1.0 / 0.0)
#endif

// directly indexed arrays of things implementation
typedef struct {
    uint32_t first_arc_index;
    uint16_t arc_count;
    float weight;
} tinyfst_state;

typedef struct {
    uint32_t input_symbol_index;
    uint32_t output_symbol_index;
    uint32_t target_state;
    float weight;
} tinyfst_arc;

typedef struct {
    tinyfst_state* states;
    tinyfst_arc* arcs;
    char** symbols;
    uint32_t statecount;
    uint32_t arccount;
    uint32_t symbolcount;
} tinyfst;

tinyfst*
tinyfst_new_initialise(uint32_t statereserve, uint32_t arcreserve,
                       uint32_t symbolreserve) {
    tinyfst* fsa = malloc(sizeof(tinyfst));
    fsa->statecount = 0;
    fsa->symbolcount = 0;
    fsa->arccount = 0;
    fsa->symbols = malloc(sizeof(char*) * symbolreserve);
    fsa->states = malloc(sizeof(tinyfst_state) * statereserve);
    fsa->arcs = malloc(sizeof(tinyfst_arc) * arcreserve);
    return fsa;
}

void
tinyfst_unreserve(tinyfst* fsa) {
    if (fsa->symbolcount > 0) {
        fsa->symbols = realloc(fsa->symbols, sizeof(char*) * fsa->symbolcount);
    }
    if (fsa->statecount > 0) {
        fsa->states = realloc(fsa->states,
                              sizeof(tinyfst_state) * fsa->statecount);
    }
    if (fsa->arccount > 0) {
        fsa->arcs = realloc(fsa->arcs,
                            sizeof(tinyfst_arc) * fsa->arccount);
    }
}

void
tinyfst_destruct(tinyfst* fsa) {
    for (uint32_t i = 0; i < fsa->symbolcount; i++) {
        free(fsa->symbols[i]);
    }
    free(fsa->symbols);
    free(fsa->states);
    free(fsa->arcs);
    free(fsa);
}

size_t
tinyfst_bytesize(tinyfst* fsa) {
    size_t size = 0;
    size += sizeof(*fsa);
    size += sizeof(tinyfst_state) * fsa->statecount;
    size += sizeof(tinyfst_arc) * fsa->arccount;
    for (uint32_t i = 0; i < fsa->symbolcount; i++) {
        size += strlen(fsa->symbols[i]) + 1;
    }
    return size;
}

void
tinyfst_print(tinyfst* fsa) {
    for (uint32_t i = 0; i < fsa->symbolcount; i++) {
        printf("%u = %s\n", i, fsa->symbols[i]);
    }
    for (uint32_t i = 0; i < fsa->statecount; i++) {
        if (fsa->states[i].weight < INFINITY) {
            printf("%u\t%f\n", i, fsa->states[i].weight);
        }
        for (uint32_t j = fsa->states[i].first_arc_index;
             j < fsa->states[i].first_arc_index + fsa->states[i].arc_count;
             j++) {
/*            printf("%u\t%u\t%s\t%s\t%f\n", i, fsa->arcs[j].target_state,
                   fsa->symbols[fsa->arcs[j].input_symbol_index],
                   fsa->symbols[fsa->arcs[j].output_symbol_index],
                   fsa->arcs[j].weight);
            XXX: printing piecemeal for debugs
*/
            printf("%u\t", i);
            printf("%u\t", fsa->arcs[j].target_state);
            printf("%s\t", fsa->symbols[fsa->arcs[j].input_symbol_index]);
            printf("%s\t", fsa->symbols[fsa->arcs[j].output_symbol_index]);
            printf("%f\n", fsa->arcs[j].weight);
        }
    }
}
#define MAX_FIELDS_IN_ATT 5

tinyfst*
tinyfst_parse_att(FILE* f) {
    uint32_t statereserve = 1024;
    uint32_t arcreserve = 1024;
    uint32_t symbolreserve = 1024;
    tinyfst* fsa = tinyfst_new_initialise(statereserve, arcreserve,
                                          symbolreserve);
    char* line = NULL;
    size_t n = 0;
    ssize_t bytes_read = 0;
    size_t linecount = 0;
    size_t current_state = 0;
    fsa->states[0].arc_count = 0;
    fsa->states[0].weight = INFINITY;
    fsa->symbols[0] = strdup("@0@");
    fsa->symbolcount = 1;
    while ((bytes_read = getline(&line,  &n, f)) > 0) {
        linecount++;
        if (line[bytes_read - 1] == '\n') {
            line[bytes_read - 1] = '\0';
        }
        char** fields = malloc(sizeof(char*) * MAX_FIELDS_IN_ATT);
        // tab separated values
        uint8_t fieldcount = 0;
        fields[fieldcount] = strtok(line, "\t");
        if (fields[fieldcount] == NULL) {
            fprintf(stderr, "%zu: empty line\n", linecount);
            tinyfst_destruct(fsa);
            free(fields);
            return NULL;
        }
        while (fields[fieldcount] != NULL) {
            fieldcount++;
            fields[fieldcount] = strtok(NULL, "\t");
        }
        if (fieldcount > 0) {
            if (fsa->statecount >= statereserve) {
                statereserve *= 2;
                fsa->states = realloc(fsa->states,
                                      sizeof(tinyfst_state) * statereserve);
            }
        }
        if (fieldcount > 2) {
            if (fsa->arccount >= arcreserve) {
                arcreserve *= 2;
                fsa->arcs = realloc(fsa->arcs,
                                    sizeof(tinyfst_arc) * arcreserve);
            }
        }
        if (fieldcount == 3) {
            fprintf(stderr, "%zu: cannot parse line with 3 columns??\n",
                    linecount);
            tinyfst_destruct(fsa);
            return NULL;
        } else if (fieldcount > 5) {
            fprintf(stderr, "%zu: cannot parse line with more than 5 columns\n",
                    linecount);
            tinyfst_destruct(fsa);
            return NULL;
        }
        // field 1 = source state
        char* endptr = fields[0];
        unsigned long statenumber = strtoul(fields[0], &endptr, 10);
        if (endptr == fields[0]) {
            fprintf(stderr, "%zu: %s not a number\n", linecount,
                    fields[0]);
            tinyfst_destruct(fsa);
            free(fields);
            return NULL;
        } else if ((*endptr != '\0') && (*endptr != '\n')) {
            fprintf(stderr, "%zu: parsing number failed at %s in line:\n%s",
                    linecount, fields[0], line);
            tinyfst_destruct(fsa);
            free(fields);
            return NULL;
        }
        if (statenumber > fsa->statecount) {
            // XXX: we kind of assume contiguous sorted state list
            fsa->statecount = statenumber;
            current_state = statenumber;
            if (fsa->statecount >= statereserve) {
                statereserve *= 2;
                fsa->states = realloc(fsa->states,
                                      sizeof(tinyfst_state) * statereserve);
            }
            fsa->states[current_state].arc_count = 0;
            fsa->states[current_state].weight = INFINITY;
        }
        if (fieldcount == 1) {
            // 1 field per line = unweighted end state = weight 0
            fsa->states[current_state].weight = 0;
            free(fields);
            continue;
        }
        // field 2 = target state or weight
        if (fieldcount == 2) {
            endptr = fields[1];
            float endweight = strtof(fields[1], &endptr);
            if (endptr == fields[1]) {
                fprintf(stderr, "%zu: %s not a float\n", linecount, fields[1]);
                tinyfst_destruct(fsa);
                free(fields);
                return NULL;
            } else if (*endptr != '\0') {
                fprintf(stderr, "%zu: parsing float failed aat %s\n", linecount,
                        fields[1]);
                tinyfst_destruct(fsa);
                free(fields);
                return NULL;
            } else {
                fsa->states[current_state].weight = endweight;
            }
            free(fields);
            continue;
        } else {
            endptr = fields[1];
            statenumber = strtoul(fields[1], &endptr, 10);
            if (endptr == fields[1]) {
                fprintf(stderr, "%zu: %s not a number\n", linecount,
                        fields[1]);
                tinyfst_destruct(fsa);
                free(fields);
                return NULL;
            } else if (*endptr != '\0') {
                fprintf(stderr, "%zu: parsing number failed at %s in line:\n%s",
                        linecount, fields[1], line);
                tinyfst_destruct(fsa);
                free(fields);
                return NULL;
            }
            if (fsa->states[current_state].arc_count == 0) {
                fsa->states[current_state].first_arc_index = fsa->arccount;
            }
            fsa->states[current_state].arc_count++;
            fsa->arcs[fsa->arccount].target_state = statenumber;
        }
        // field 3 & 4 input output
        bool known_symbol = false;
        for (uint16_t i = 0; i < fsa->symbolcount; i++) {
            if (strcmp(fsa->symbols[i], fields[2]) == 0) {
                known_symbol = true;
                fsa->arcs[fsa->arccount].input_symbol_index = i;
                break;
            }
        }
        if (known_symbol == false) {
            fsa->symbols[fsa->symbolcount] = strdup(fields[2]);
            fsa->arcs[fsa->arccount].input_symbol_index = fsa->symbolcount;
            fsa->symbolcount++;
        }
        known_symbol = false;
        if (fields[3][strlen(fields[3]) - 1] == '\n') {
            fields[3][strlen(fields[3]) - 1] = '\0';
        }
        for (uint16_t i = 0; i < fsa->symbolcount; i++) {
            if (strcmp(fsa->symbols[i], fields[3]) == 0) {
                known_symbol = true;
                fsa->arcs[fsa->arccount].output_symbol_index = i;
                break;
            }
        }
        if (known_symbol == false) {
            fsa->symbols[fsa->symbolcount] = strdup(fields[3]);
            fsa->arcs[fsa->arccount].output_symbol_index = fsa->symbolcount;
            fsa->symbolcount++;
        }
        // field 5 arc weight
        if (fieldcount == 5) {
            endptr = fields[4];
            float transweight = strtod(fields[4], &endptr);
            if (endptr == fields[4]) {
                fprintf(stderr, "%zu: %s not a float\n", linecount, fields[4]);
                tinyfst_destruct(fsa);
                free(fields);
                return NULL;
            } else if (*endptr != '\0') {
                fprintf(stderr, "%zu: parsing float failed aat %s\n", linecount,
                        fields[4]);
                tinyfst_destruct(fsa);
                free(fields);
                return NULL;
            } else {
                fsa->arcs[fsa->arccount].weight = transweight;
            }
        } else {
            fsa->arcs[fsa->arccount].weight = 0;
        }
        if (fieldcount > 2) {
            fsa->arccount++;
        }
        free(fields);
    }
    free(line);
    tinyfst_unreserve(fsa);
    return fsa;
}

int
main(int argc, char** argv) {
    if (argc != 2) {
        fprintf(stderr, "Usage: %s ATTFILE\n", argv[0]);
        exit(2);
    }
    FILE* f = fopen(argv[1], "r");
    tinyfst* fsa = tinyfst_parse_att(f);
    if (fsa == NULL) {
        fprintf(stderr, "Parsing %s failed\n", argv[1]);
        exit(1);
    }
    fprintf(stdout, "Read FSA: %u states, %u arcs, %u symbols\n",
            fsa->statecount, fsa->arccount, fsa->symbolcount);
    //tinyfst_print(fsa);
    fprintf(stdout, "%zu bytes\n", tinyfst_bytesize(fsa));
    fprintf(stdout, "(paused for htop) ");
    getchar();
    tinyfst_destruct(fsa);
    fclose(f);
}
