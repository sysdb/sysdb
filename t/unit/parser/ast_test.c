/*
 * SysDB - t/unit/parser/ast_test.c
 * Copyright (C) 2015 Sebastian 'tokkee' Harl <sh@tokkee.org>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDERS OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#if HAVE_CONFIG_H
#	include "config.h"
#endif

#include "parser/ast.h"
#include "parser/parser.h"
#include "testutils.h"

#include <check.h>

/*
 * tests
 */

START_TEST(test_init)
{
	/* simple test to check that the initializers match;
	 * not all of them are used at all times */
	sdb_ast_op_t op = SDB_AST_OP_INIT;
	sdb_ast_iter_t iter = SDB_AST_ITER_INIT;
	sdb_ast_typed_t typed = SDB_AST_TYPED_INIT;
	sdb_ast_const_t constant = SDB_AST_CONST_INIT;
	sdb_ast_value_t value = SDB_AST_VALUE_INIT;
	sdb_ast_fetch_t fetch = SDB_AST_FETCH_INIT;
	sdb_ast_list_t list = SDB_AST_LIST_INIT;
	sdb_ast_lookup_t lookup = SDB_AST_LOOKUP_INIT;
	sdb_ast_store_t store = SDB_AST_STORE_INIT;
	sdb_ast_timeseries_t ts = SDB_AST_TIMESERIES_INIT;

	/* do some (mostly) dummy operation */
	sdb_parser_analyze(SDB_AST_NODE(&op), NULL);
	sdb_parser_analyze(SDB_AST_NODE(&iter), NULL);
	sdb_parser_analyze(SDB_AST_NODE(&typed), NULL);
	sdb_parser_analyze(SDB_AST_NODE(&constant), NULL);
	sdb_parser_analyze(SDB_AST_NODE(&value), NULL);
	sdb_parser_analyze(SDB_AST_NODE(&fetch), NULL);
	sdb_parser_analyze(SDB_AST_NODE(&list), NULL);
	sdb_parser_analyze(SDB_AST_NODE(&lookup), NULL);
	sdb_parser_analyze(SDB_AST_NODE(&store), NULL);
	sdb_parser_analyze(SDB_AST_NODE(&ts), NULL);
}
END_TEST

TEST_MAIN("parser::ast")
{
	TCase *tc = tcase_create("core");
	tcase_add_test(tc, test_init);
	ADD_TCASE(tc);
}
TEST_MAIN_END

/* vim: set tw=78 sw=4 ts=4 noexpandtab : */

