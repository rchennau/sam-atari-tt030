#ifndef RUNNER_H
#define RUNNER_H

#ifdef __cplusplus
extern "C" {
#endif

int runner_allowed(char *const argv[]);
int runner_start(char *const argv[]);
long runner_poll(char *buf, long cap);
int runner_finish(void);
void runner_cancel(void);
int runner_is_running(void);

#ifdef __cplusplus
}
#endif

#endif /* RUNNER_H */
