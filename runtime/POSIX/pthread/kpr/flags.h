#ifndef KPR_FLAGS_H
#define KPR_FLAGS_H

#define KPR_THREAD_STATE_LIVE (0)
#define KPR_THREAD_STATE_DEAD (1)
#define KPR_THREAD_STATE_EXITED (2)

#define KPR_THREAD_MODE_JOIN (0)
#define KPR_THREAD_MODE_DETACH (1)

#define KPR_THREAD_JSTATE_JOINABLE (0)
#define KPR_THREAD_JSTATE_WAIT_FOR_JOIN (1)
#define KPR_THREAD_JSTATE_JOINED (2)

#define KPR_MUTEX_NORMAL (0)
#define KPR_MUTEX_INCONSISTENT (1)
#define KPR_MUTEX_UNUSABLE (2)

#endif // KPR_FLAGS_H