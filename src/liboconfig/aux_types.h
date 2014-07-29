#ifndef SDB_OCONFIG_AUX_TYPES_H
#define SDB_OCONFIG_AUX_TYPES_H 1

struct statement_list_s
{
	oconfig_item_t *statement;
	int             statement_num;
};
typedef struct statement_list_s statement_list_t;

struct argument_list_s
{
	oconfig_value_t *argument;
	int              argument_num;
};
typedef struct argument_list_s argument_list_t;

#endif /* SDB_OCONFIG_AUX_TYPES_H */
