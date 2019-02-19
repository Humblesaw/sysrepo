/**
 * @file test_modules.c
 * @author Michal Vasko <mvasko@cesnet.cz>
 * @brief test for adding/removing modules
 *
 * @copyright
 * Copyright (c) 2019 CESNET, z.s.p.o.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *    http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
#define _GNU_SOURCE

#include <unistd.h>
#include <setjmp.h>
#include <string.h>
#include <stdarg.h>

#include <cmocka.h>
#include <libyang/libyang.h>

#include "tests/config.h"
#include "sysrepo.h"

struct state {
    sr_conn_ctx_t *conn;
};

static int
setup_f(void **state)
{
    struct state *st;

    st = calloc(1, sizeof *st);
    *state = st;

    if (sr_connect("test1", 0, &st->conn) != SR_ERR_OK) {
        return 1;
    }

    return 0;
}

static int
teardown_f(void **state)
{
    struct state *st = (struct state *)*state;

    sr_disconnect(st->conn);
    free(st);
    return 0;
}

static void
cmp_int_data(sr_conn_ctx_t *conn, const char *module_name, const char *expected)
{
    char *str, buf[1024];
    struct lyd_node *data;
    struct ly_set *set;
    int ret;

    /* parse internal data */
    sprintf(buf, "%s/data/sysrepo.startup", sr_get_repo_path());
    data = lyd_parse_path((struct ly_ctx *)sr_get_context(conn), buf, LYD_LYB, LYD_OPT_CONFIG);
    assert_non_null(data);

    /* filter the module */
    sprintf(buf, "/sysrepo:sysrepo-modules/module[name='%s']", module_name);
    set = lyd_find_path(data, buf);
    assert_non_null(set);
    assert_int_equal(set->number, 1);

    /* check current internal (sorted) data */
    ret = lyd_schema_sort(set->set.d[0], 1);
    assert_int_equal(ret, 0);
    ret = lyd_print_mem(&str, set->set.d[0], LYD_XML, 0);
    ly_set_free(set);
    lyd_free_withsiblings(data);
    assert_int_equal(ret, 0);

    assert_string_equal(str, expected);
    free(str);
}

static void
test_data_deps(void **state)
{
    struct state *st = (struct state *)*state;
    int ret;

    ret = sr_install_module(st->conn, TESTS_DIR "/files/test.yang", TESTS_DIR "/files", NULL, 0, 0);
    assert_int_equal(ret, SR_ERR_OK);
    ret = sr_install_module(st->conn, TESTS_DIR "/files/ietf-interfaces.yang", TESTS_DIR "/files", NULL, 0, 1);
    assert_int_equal(ret, SR_ERR_OK);
    ret = sr_install_module(st->conn, TESTS_DIR "/files/iana-if-type.yang", TESTS_DIR "/files", NULL, 0, 0);
    assert_int_equal(ret, SR_ERR_OK);
    ret = sr_install_module(st->conn, TESTS_DIR "/files/refs.yang", TESTS_DIR "/files", NULL, 0, 1);
    assert_int_equal(ret, SR_ERR_OK);

    ret = sr_remove_module(st->conn, "refs");
    assert_int_equal(ret, SR_ERR_OK);
    ret = sr_remove_module(st->conn, "ietf-interfaces");
    assert_int_equal(ret, SR_ERR_OK);
    ret = sr_remove_module(st->conn, "iana-if-type");
    assert_int_equal(ret, SR_ERR_OK);
    ret = sr_remove_module(st->conn, "test");
    assert_int_equal(ret, SR_ERR_OK);

    /* check current internal data */
    cmp_int_data(st->conn, "test",
    "<module xmlns=\"urn:sysrepo\">"
        "<name>test</name>"
        "<has-data/>"
        "<removed/>"
    "</module>"
    );
    cmp_int_data(st->conn, "ietf-interfaces",
    "<module xmlns=\"urn:sysrepo\">"
        "<name>ietf-interfaces</name>"
        "<revision>2014-05-08</revision>"
        "<has-data/>"
        "<replay-support/>"
        "<removed/>"
    "</module>"
    );
    cmp_int_data(st->conn, "iana-if-type",
    "<module xmlns=\"urn:sysrepo\">"
        "<name>iana-if-type</name>"
        "<revision>2014-05-08</revision>"
        "<removed/>"
    "</module>"
    );
    cmp_int_data(st->conn, "refs",
    "<module xmlns=\"urn:sysrepo\">"
        "<name>refs</name>"
        "<has-data/>"
        "<replay-support/>"
        "<removed/>"
        "<data-deps>"
            "<module>test</module>"
            "<inst-id>"
                "<xpath xmlns:r=\"urn:refs\">/r:cont/r:def-inst-id</xpath>"
                "<default-module>test</default-module>"
            "</inst-id>"
            "<inst-id>"
                "<xpath xmlns:r=\"urn:refs\">/r:inst-id</xpath>"
            "</inst-id>"
        "</data-deps>"
    "</module>"
    );
}

static void
test_op_deps(void **state)
{
    struct state *st = (struct state *)*state;
    int ret;

    ret = sr_install_module(st->conn, TESTS_DIR "/files/ops-ref.yang", TESTS_DIR "/files", NULL, 0, 1);
    assert_int_equal(ret, SR_ERR_OK);
    ret = sr_install_module(st->conn, TESTS_DIR "/files/ops.yang", TESTS_DIR "/files", NULL, 0, 0);
    assert_int_equal(ret, SR_ERR_OK);

    ret = sr_remove_module(st->conn, "ops");
    assert_int_equal(ret, SR_ERR_OK);
    ret = sr_remove_module(st->conn, "ops-ref");
    assert_int_equal(ret, SR_ERR_OK);

    /* check current internal data */
    cmp_int_data(st->conn, "ops-ref",
    "<module xmlns=\"urn:sysrepo\">"
        "<name>ops-ref</name>"
        "<has-data/>"
        "<replay-support/>"
        "<removed/>"
    "</module>"
    );

    cmp_int_data(st->conn, "ops",
    "<module xmlns=\"urn:sysrepo\">"
        "<name>ops</name>"
        "<has-data/>"
        "<removed/>"
        "<op-deps>"
            "<xpath xmlns:o=\"urn:ops\">/o:rpc1</xpath>"
            "<in>"
                "<module>ops-ref</module>"
                "<inst-id>"
                    "<xpath xmlns:o=\"urn:ops\">/o:rpc1/o:l2</xpath>"
                    "<default-module>ops-ref</default-module>"
                "</inst-id>"
            "</in>"
        "</op-deps>"
        "<op-deps>"
            "<xpath xmlns:o=\"urn:ops\">/o:rpc2</xpath>"
            "<out>"
                "<module>ops-ref</module>"
            "</out>"
        "</op-deps>"
        "<op-deps>"
            "<xpath xmlns:o=\"urn:ops\">/o:rpc3</xpath>"
        "</op-deps>"
        "<op-deps>"
            "<xpath xmlns:o=\"urn:ops\">/o:cont/o:list1/o:cont2/o:act1</xpath>"
            "<out>"
                "<module>ops</module>"
                "<inst-id>"
                    "<xpath xmlns:o=\"urn:ops\">/o:cont/o:list1/o:cont2/o:act1/o:l8</xpath>"
                    "<default-module>ops</default-module>"
                "</inst-id>"
            "</out>"
        "</op-deps>"
        "<op-deps>"
            "<xpath xmlns:o=\"urn:ops\">/o:cont/o:list1/o:act2</xpath>"
        "</op-deps>"
        "<op-deps>"
            "<xpath xmlns:o=\"urn:ops\">/o:cont/o:cont3/o:notif2</xpath>"
            "<in>"
                "<inst-id>"
                    "<xpath xmlns:o=\"urn:ops\">/o:cont/o:cont3/o:notif2/o:l13</xpath>"
                "</inst-id>"
            "</in>"
        "</op-deps>"
        "<op-deps>"
            "<xpath xmlns:o=\"urn:ops\">/o:notif3</xpath>"
            "<in>"
                "<module>ops-ref</module>"
                "<inst-id>"
                    "<xpath xmlns:o=\"urn:ops\">/o:notif3/o:list2/o:l15</xpath>"
                    "<default-module>ops</default-module>"
                "</inst-id>"
            "</in>"
        "</op-deps>"
    "</module>"
    );
}

int
main(void)
{
    const struct CMUnitTest tests[] = {
        cmocka_unit_test_setup_teardown(test_data_deps, setup_f, teardown_f),
        cmocka_unit_test_setup_teardown(test_op_deps, setup_f, teardown_f),
    };

    sr_log_stderr(SR_LL_INF);
    return cmocka_run_group_tests(tests, NULL, NULL);
}
