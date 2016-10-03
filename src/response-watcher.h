#ifndef RESPONSE_WATCHER_H
#define RESPONSE_WATCHER_H

#include <glib.h>
#include <glib-object.h>
#include <pthread.h>

#include "message-queue.h"

G_BEGIN_DECLS

typedef struct _ResponseWatcherClass {
    GObjectClass parent;
} ResponseWatcherClass;

/** DON'T TOUCH!
 * Direct access to data in this structure his highly discouraged. Use the
 * functions in this header to interact with this "object". If you're tempted
 * to access the structure directly you probably need to update the API.
 */
typedef struct _ResponseWatcher {
    GObject            parent_instance;
    MessageQueue      *in_queue;
    pthread_t          thread;
} ResponseWatcher;

#define TYPE_RESPONSE_WATCHER              (response_watcher_get_type ())
#define RESPONSE_WATCHER(obj)              (G_TYPE_CHECK_INSTANCE_CAST ((obj),   TYPE_RESPONSE_WATCHER, ResponseWatcher))
#define RESPONSE_WATCHER_CLASS(klass)      (G_TYPE_CHECK_CLASS_CAST    ((klass), TYPE_RESPONSE_WATCHER, ResponseWatcherClass))
#define IS_RESPONSE_WATCHER(obj)           (G_TYPE_CHECK_INSTANCE_TYPE ((obj),   TYPE_RESPONSE_WATCHER))
#define IS_RESPONSE_WATCHER_CLASS(klass)   (G_TYPE_CHECK_CLASS_TYPE    ((klass), TYPE_RESPONSE_WATCHER))
#define RESPONSE_WATCHER_GET_CLASS(obj)    (G_TYPE_INSTANCE_GET_CLASS  ((obj),   TYPE_RESPONSE_WATCHER, ResponseWatcherClass))

GType               response_watcher_get_type    (void);
ResponseWatcher*    response_watcher_new         (void);
void                response_watcher_enqueue     (ResponseWatcher   *watcher,
                                                  GObject           *obj);

G_END_DECLS
#endif /* RESPONSE_WATCHER_H */
