#include <gio/gunixfdlist.h>
#include <glib.h>
#include <inttypes.h>
#include <stdio.h>

#include <setjmp.h>
#include <cmocka.h>

#include <sapi/tpm20.h>

#include "tcti-tabrmd.h"
#include "tcti-tabrmd-priv.h"

/*
 * when given an NULL context and a pointer to a size_t, set the size_t
 * parameter to the size of the TSS2_TCTI_TABRMD_CONTEXT structure.
 */
static void
tcti_tabrmd_init_size_test (void **state)
{
    size_t tcti_size;

    tss2_tcti_tabrmd_init (NULL, &tcti_size);
    assert_int_equal (tcti_size, sizeof (TSS2_TCTI_TABRMD_CONTEXT));
}
/*
 * when given a NULL context and a pointer to size_t, the init function
 * returns TSS2_RC_SUCCESS
 */
static void
tcti_tabrmd_init_success_return_value_test (void **state)
{
    size_t tcti_size;
    TSS2_RC ret;

    ret = tss2_tcti_tabrmd_init (NULL, &tcti_size);
    assert_int_equal (ret, TSS2_RC_SUCCESS);
}
/*
 * when given NULL pointers for both parameters the init function returns
 * an error indicating that the values are bad (TSS2_TCTI_RC_BAD_VALUE)
 */
static void
tcti_tabrmd_init_allnull_is_bad_value_test (void **state)
{
    TSS2_RC ret;

    ret = tss2_tcti_tabrmd_init (NULL, NULL);
    assert_int_equal (ret, TSS2_TCTI_RC_BAD_VALUE);
}
/*
 * This is a mock function to control return values from the connection
 * creation logic that invokes the DBus "CreateConnection" function exposed
 * by the tabrmd. This is called by the tcti as part of initializing the
 * TCTI context.
 */
GVariant *
__wrap_g_dbus_proxy_call_with_unix_fd_list_sync (
    GDBusProxy *proxy,
    const gchar *method_name,
    GVariant *parameters,
    GDBusCallFlags flags,
    gint timeout_msec,
    GUnixFDList *fd_list,
    GUnixFDList **out_fd_list,
    GCancellable *cancellable,
    GError **error)
{
    GVariant *variant_array[2] = { 0 }, *variant_tuple;
    gint32 fds[2];
    guint64 id;

    fds[0] = (gint32)mock ();
    fds[1] = (gint32)mock ();
    id = (guint64)mock ();

    *out_fd_list = g_unix_fd_list_new_from_array (fds, 2);
    variant_array[0] = g_variant_new_fixed_array (G_VARIANT_TYPE ("h"),
                                                  fds,
                                                  2,
                                                  sizeof (gint32));
    variant_array[1] = g_variant_new_uint64 (id);
    variant_tuple = g_variant_new_tuple (variant_array, 2);

    return variant_tuple;
}
/*
 * This is a mock function to control behavior of the
 * tcti_tabrmd_call_cancel_sync function. This is a function generated from
 * the DBus xml specification. It returns only a TSS2_RC.
 */
gboolean
__wrap_tcti_tabrmd_call_cancel_sync (
    TctiTabrmd *proxy,
    guint64 arg_id,
    guint *out_return_code,
    GCancellable *cancellable,
    GError **error)
{
    *out_return_code = (guint)mock ();
    return (gboolean)mock ();
}
/*
 * This is a mock function to control behavior of the
 * tcti_tabrmd_call_set_locality_sync function. This is a function generated from
 * the DBus xml specification. It returns only a TSS2_RC.
 */
gboolean
__wrap_tcti_tabrmd_call_set_locality_sync (
    TctiTabrmd *proxy,
    guint64 arg_id,
    guchar arg_locality,
    guint *out_return_code,
    GCancellable *cancellable,
    GError **error)
{
    *out_return_code = (guint)mock ();
    return (gboolean)mock ();
}
/*
 * Structure to hold data relevant to tabrmd TCTI unit tests.
 */
typedef struct {
    guint64 id;
    gint32  fd_receive_client;
    gint32  fd_receive_server;
    gint32  fd_transmit_client;
    gint32  fd_transmit_server;
    TSS2_TCTI_CONTEXT  *context;
} data_t;
/*
 * This is the setup function used to create the TCTI context structure for
 * tests that require the initialization be done before the test can be
 * executed. It *must* be paired with a call to the teardown function.
 */
void
tcti_tabrmd_setup (void **state)
{
    data_t *data;
    TSS2_RC ret = TSS2_RC_SUCCESS;
    size_t tcti_size = 0;
    gint32 fds[2] = { 0 };
    guint64 id = 666;

    data = calloc (1, sizeof (data_t));
    ret = tss2_tcti_tabrmd_init (NULL, &tcti_size);
    if (ret != TSS2_RC_SUCCESS) {
        printf ("tss2_tcti_tabrmd_init failed: %d\n", ret);
        return;
    }
    data->context = calloc (1, tcti_size);
    if (data->context == NULL) {
        perror ("calloc");
        return;
    }
    g_debug ("preparing g_dbus_proxy_call_with_unix_fd_list_sync mock wrapper");
    assert_int_equal (pipe (fds), 0);
    data->fd_receive_client  = fds [0];
    data->fd_transmit_server = fds [1];
    assert_int_equal (pipe (fds), 0);
    data->fd_receive_server  = fds [0];
    data->fd_transmit_client = fds [1];
    will_return (__wrap_g_dbus_proxy_call_with_unix_fd_list_sync,
                 data->fd_receive_client);
    will_return (__wrap_g_dbus_proxy_call_with_unix_fd_list_sync,
                 data->fd_transmit_client);
    data->id = id;
    will_return (__wrap_g_dbus_proxy_call_with_unix_fd_list_sync, id);
    g_debug ("about to call real tss2_tcti_tabrmd_init function");
    ret = tss2_tcti_tabrmd_init (data->context, 0);
    assert_int_equal (ret, TSS2_RC_SUCCESS);

    *state = data;
}
/*
 * This is a teardown function to deallocate / cleanup all resources
 * associated with these tests.
 */
void
tcti_tabrmd_teardown (void **state)
{
    data_t *data = *state;

    tss2_tcti_finalize (data->context);
    close (data->fd_receive_client);
    close (data->fd_receive_server);
    close (data->fd_transmit_client);
    close (data->fd_transmit_server);
    if (data->context)
        free (data->context);
    free (data);
}
/*
 * Ensure that after initialization the 'magic' value in the TCTI structure
 * is the one that we expect.
 */
static void
tcti_tabrmd_magic_test (void **state)
{
    data_t *data = *state;

    assert_int_equal (TSS2_TCTI_MAGIC (data->context),
                      TSS2_TCTI_TABRMD_MAGIC);
}
/*
 * Ensure that after initialization the 'version' value in the TCTI structure
 * is the one that we expect.
 */
static void
tcti_tabrmd_version_test (void **state)
{
    data_t *data = *state;

    assert_int_equal (TSS2_TCTI_VERSION (data->context), 1);
}
/*
 * Ensure that after initialization the 'id' value set for the connection is
 * the one taht we expect.
 */
static void
tcti_tabrmd_init_success_test (void **state)
{
    data_t *data;

    tcti_tabrmd_setup (state);
    data = *state;
    assert_int_equal (data->id, TSS2_TCTI_TABRMD_ID (data->context));
    tcti_tabrmd_teardown (state);
}
/*
 * This test makes a single call to the transmit function in the the
 * TSS2_TCTI_CONTEXT function pointer table. It sends a fixed 12 byte TPM
 * command buffer:
 * tag:     TPM_ST_NO_SESSIONS
 * size:    12 bytes
 * command: 0x0
 * data:    0x01, 0x02
 */
static void
tcti_tabrmd_transmit_success_test (void **state)
{
    data_t *data = *state;
    uint8_t command_in [] = { 0x80, 0x02,
                              0x00, 0x00, 0x00, 0x0c,
                              0x00, 0x00, 0x00, 0x00,
                              0x01, 0x02};
    size_t size = sizeof (command_in);
    uint8_t command_out [sizeof (command_in)] = { 0 };
    TSS2_RC rc;
    ssize_t ret;

    rc = tss2_tcti_transmit (data->context, size, command_in);
    assert_int_equal (rc, TSS2_RC_SUCCESS);
    ret = read (data->fd_receive_server, command_out, size);
    assert_int_equal (ret, size);
    assert_memory_equal (command_in, command_out, size);
}
/*
 * This test ensures that the magic value in the context structure is checked
 * before the transmit function executes and that the RC is what we expect.
 */
static void
tcti_tabrmd_transmit_bad_magic_test (void **state)
{
    data_t *data = *state;
    TSS2_RC rc;
    uint8_t command [12] = { 0 };
    size_t size = 12;

    TSS2_TCTI_MAGIC (data->context) = 1;
    rc = tss2_tcti_transmit (data->context, size, command);
    assert_int_equal (rc, TSS2_TCTI_RC_BAD_CONTEXT);
}
/*
 * This test ensures that the version in the context structure is checked
 * before the transmit function executes and that the RC is what we expect.
 */
static void
tcti_tabrmd_transmit_bad_version_test (void **state)
{
    data_t *data = *state;
    TSS2_RC rc;
    uint8_t command [12] = { 0 };
    size_t size = 12;

    TSS2_TCTI_VERSION (data->context) = -1;
    rc = tss2_tcti_transmit (data->context, size, command);
    assert_int_equal (rc, TSS2_TCTI_RC_BAD_CONTEXT);
}
/*
 * This test makes a single call to the receive function in the
 * TSS2_TCTI_CONTEXT function pointer table.
 */
static void
tcti_tabrmd_receive_success_test (void **state)
{
    data_t  *data = *state;
    uint8_t command_in [] = { 0x80, 0x02,
                              0x00, 0x00, 0x00, 0x0c,
                              0x00, 0x00, 0x00, 0x00,
                              0x01, 0x02};
    size_t size = sizeof (command_in);
    uint8_t command_out [sizeof (command_in)] = { 0 };
    TSS2_RC rc;
    ssize_t ret;

    ret = write (data->fd_transmit_server, command_in, size);
    assert_int_equal (ret, size);
    rc = tss2_tcti_receive (data->context,
                            &size,
                            command_out,
                            TSS2_TCTI_TIMEOUT_BLOCK);
    assert_int_equal (rc, TSS2_RC_SUCCESS);
    assert_int_equal (size, sizeof (command_in));
    assert_memory_equal (command_in, command_out, size);
}
/*
 * This test ensures that the magic value in the context structure is checked
 * before the receive function executes and that the RC is what we expect.
 */
static void
tcti_tabrmd_receive_bad_magic_test (void **state)
{
    data_t *data = *state;
    TSS2_RC rc;
    uint8_t buf [12] = { 0 };
    size_t size = 12;

    TSS2_TCTI_MAGIC (data->context) = 1;
    rc = tss2_tcti_receive (data->context, &size, buf, TSS2_TCTI_TIMEOUT_BLOCK);
    assert_int_equal (rc, TSS2_TCTI_RC_BAD_CONTEXT);
}
/*
 * This test ensures that the version in the context structure is checked
 * before the transmit function executes and that the RC is what we expect.
 */
static void
tcti_tabrmd_receive_bad_version_test (void **state)
{
    data_t *data = *state;
    TSS2_RC rc;
    uint8_t buf [12] = { 0 };
    size_t size = 12;

    TSS2_TCTI_VERSION (data->context) = -1;
    rc = tss2_tcti_receive (data->context, &size, buf, TSS2_TCTI_TIMEOUT_BLOCK);
    assert_int_equal (rc, TSS2_TCTI_RC_BAD_CONTEXT);
}
/*
 * This test makes a single call to the receive function in the
 * TSS2_TCTI_CONTEXT function pointer table with a relatively short
 * timeout.
 *
 * NOTE: currently this is unimplemented.
 */
static void
tcti_tabrmd_receive_timeout_test (void **state)
{
    data_t  *data = *state;
    uint8_t command_in [] = { 0x80, 0x02,
                              0x00, 0x00, 0x00, 0x0c,
                              0x00, 0x00, 0x00, 0x00,
                              0x01, 0x02};
    size_t size = sizeof (command_in);
    uint8_t command_out [sizeof (command_in)] = { 0 };
    TSS2_RC rc;

    rc = tss2_tcti_receive (data->context,
                            &size,
                            command_out,
                            1000);
    assert_int_equal (rc, TSS2_TCTI_RC_BAD_VALUE);
}
/*
 */
static void
tcti_tabrmd_receive_size_lt_header_test (void **state)
{
    data_t *data = *state;
    uint8_t response [TPM_HEADER_SIZE] = { 0 };
    size_t size = sizeof (response) - 1;
    TSS2_RC rc;

    rc = tss2_tcti_receive (data->context,
                            &size,
                            response,
                            TSS2_TCTI_TIMEOUT_BLOCK);
    assert_int_equal (rc, TSS2_TCTI_RC_INSUFFICIENT_BUFFER);
}
static void
tcti_tabrmd_receive_size_lt_body_test (void **state)
{
    data_t *data = *state;
    uint8_t response [] = { 0x80, 0x02,
                            0x00, 0x00, 0x00, 0x0e,
                            0x00, 0x00, 0x00, 0x00,
                            0x00, 0x00, 0x00, 0x00 };
    size_t size = sizeof (response);
    TSS2_RC rc;
    ssize_t written;

    written = write (data->fd_transmit_server, response, size);
    assert_int_equal (written, size);
    size -= 1; /* tell the receive function we have less space */
    rc = tss2_tcti_receive (data->context,
                            &size,
                            response,
                            TSS2_TCTI_TIMEOUT_BLOCK);
    assert_int_equal (rc, TSS2_TCTI_RC_INSUFFICIENT_BUFFER);
}
/*
 * This test sets up the call_cancel mock function to return values
 * indicating success. It then ensures that an invocation of the cancel
 * convenience macro invokes the underlying function correctly.
 */
static void
tcti_tabrmd_cancel_test (void **state)
{
    data_t *data = *state;
    TSS2_RC rc;

    will_return (__wrap_tcti_tabrmd_call_cancel_sync, TSS2_RC_SUCCESS);
    will_return (__wrap_tcti_tabrmd_call_cancel_sync, TRUE);
    rc = tss2_tcti_cancel (data->context);
    assert_int_equal (rc, TSS2_RC_SUCCESS);
}
/*
 * This test invokes the get_poll_handles function. It then ensures that
 * it returns the expected NOT_IMPLEMENTED RC.
 */
static void
tcti_tabrmd_get_poll_handles_test (void **state)
{
    data_t *data = *state;
    TSS2_TCTI_POLL_HANDLE handles[2];
    size_t num_handles = 2;
    TSS2_RC rc;

    rc = tss2_tcti_get_poll_handles (data->context, handles, &num_handles);
    assert_int_equal (rc, TSS2_TCTI_RC_NOT_IMPLEMENTED);
}
/*
 * This test sets up the call_set_locality mock function to return values
 * indicating success. It then ensures that an invocation of the set_locality
 * convenience macro invokes the underlying function correctly.
 */
static void
tcti_tabrmd_set_locality_test (void **state)
{
    data_t *data = *state;
    uint8_t locality;
    TSS2_RC rc;

    will_return (__wrap_tcti_tabrmd_call_set_locality_sync, TSS2_RC_SUCCESS);
    will_return (__wrap_tcti_tabrmd_call_set_locality_sync, TRUE);
    rc = tss2_tcti_set_locality (data->context, locality);
    assert_int_equal (rc, TSS2_RC_SUCCESS);
}
int
main(int argc, char* argv[])
{
    const UnitTest tests[] = {
        unit_test (tcti_tabrmd_init_size_test),
        unit_test (tcti_tabrmd_init_success_return_value_test),
        unit_test (tcti_tabrmd_init_allnull_is_bad_value_test),
        unit_test (tcti_tabrmd_init_success_test),
        unit_test_setup_teardown (tcti_tabrmd_magic_test,
                                  tcti_tabrmd_setup,
                                  tcti_tabrmd_teardown),
        unit_test_setup_teardown (tcti_tabrmd_version_test,
                                  tcti_tabrmd_setup,
                                  tcti_tabrmd_teardown),
        unit_test_setup_teardown (tcti_tabrmd_transmit_success_test,
                                  tcti_tabrmd_setup,
                                  tcti_tabrmd_teardown),
        unit_test_setup_teardown (tcti_tabrmd_transmit_bad_magic_test,
                                  tcti_tabrmd_setup,
                                  tcti_tabrmd_teardown),
        unit_test_setup_teardown (tcti_tabrmd_transmit_bad_version_test,
                                  tcti_tabrmd_setup,
                                  tcti_tabrmd_teardown),
        unit_test_setup_teardown (tcti_tabrmd_receive_success_test,
                                  tcti_tabrmd_setup,
                                  tcti_tabrmd_teardown),
        unit_test_setup_teardown (tcti_tabrmd_receive_bad_magic_test,
                                  tcti_tabrmd_setup,
                                  tcti_tabrmd_teardown),
        unit_test_setup_teardown (tcti_tabrmd_receive_bad_version_test,
                                  tcti_tabrmd_setup,
                                  tcti_tabrmd_teardown),
        unit_test_setup_teardown (tcti_tabrmd_receive_timeout_test,
                                  tcti_tabrmd_setup,
                                  tcti_tabrmd_teardown),
        unit_test_setup_teardown (tcti_tabrmd_receive_size_lt_header_test,
                                  tcti_tabrmd_setup,
                                  tcti_tabrmd_teardown),
        unit_test_setup_teardown (tcti_tabrmd_receive_size_lt_body_test,
                                  tcti_tabrmd_setup,
                                  tcti_tabrmd_teardown),
         unit_test_setup_teardown (tcti_tabrmd_cancel_test,
                                  tcti_tabrmd_setup,
                                  tcti_tabrmd_teardown),
        unit_test_setup_teardown (tcti_tabrmd_get_poll_handles_test,
                                  tcti_tabrmd_setup,
                                  tcti_tabrmd_teardown),
        unit_test_setup_teardown (tcti_tabrmd_set_locality_test,
                                  tcti_tabrmd_setup,
                                  tcti_tabrmd_teardown),
    };
    return run_tests(tests);
}