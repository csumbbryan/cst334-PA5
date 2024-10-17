
#include "src/student_code.h"
#include "src/server.h"
#include "src/database.h"
#include "src/common.h"

#include <criterion/criterion.h>
#include <unistd.h>
#include "stdio.h"

typedef struct conds_args_t {
  float delay;
  PlayerDatabase* db;
} conds_args_t;

void* signal_read_after_delay(void* args) {
  PlayerDatabase* db = ((conds_args_t*)args)->db;
  float delay = ((conds_args_t*)args)->delay;

  // Check whether we are in fact respecting MAX_CONCURRENT_READERS
  fsleep(1.1 * delay);
  pthread_mutex_lock(db->mutex);
  db->readers = MAX_CONCURRENT_READERS;
  db->writers = 0;
  pthread_cond_signal(db->can_read);
  pthread_mutex_unlock(db->mutex);

  // Check whether we are respecting when there are writers
  fsleep(1.1 * delay);
  pthread_mutex_lock(db->mutex);
  db->readers = 0;
  db->writers = 1;
  pthread_cond_signal(db->can_read);
  pthread_mutex_unlock(db->mutex);

  // Finally, this should succeed
  fsleep(1.1 * delay);
  pthread_mutex_lock(db->mutex);
  db->readers = 0;
  db->writers = 0;
  pthread_cond_signal(db->can_read);
  pthread_mutex_unlock(db->mutex);

  return NULL;
}

void* signal_write_after_delay(void* args) {
  PlayerDatabase* db = ((conds_args_t*)args)->db;
  float delay = ((conds_args_t*)args)->delay;

  // Check whether we are in fact respecting readers
  fsleep(1.1 * delay);
//  log_debug("signal_write_after_delay: test readers\n")
  pthread_mutex_lock(db->mutex);
  db->readers = 1;
  db->writers = 0;
  pthread_cond_signal(db->can_write);
  pthread_mutex_unlock(db->mutex);

  // Check whether we are respecting when there are writers
  fsleep(1.1 * delay);
//  log_debug("signal_write_after_delay: test writers\n")
  pthread_mutex_lock(db->mutex);
  db->readers = 0;
  db->writers = 1;
  pthread_cond_signal(db->can_write);
  pthread_mutex_unlock(db->mutex);

  // Finally, this should succeed
  fsleep(1.1 * delay);
  pthread_mutex_lock(db->mutex);
//  log_debug("signal_write_after_delay: test good\n")
  db->readers = 0;
  db->writers = 0;
  pthread_cond_signal(db->can_write);
  pthread_mutex_unlock(db->mutex);

  return NULL;
}

int main() {
  pthread_create(&server_thread, NULL, run_server, "5005");
  double start_time,end_time;
  start_time = currentTime();

log_debug("Testing server increment_test_two_users_mixed_workload....\n")
  pthread_t* threads[NUM_PLAYS];
  make_request("add_player sam0");
  make_request("add_player sam1");
  double time_start = currentTime();

  char msg[NUM_PLAYS][100];
  int high_score_0 = 0;
  int high_score_1 = 0;
  int plays_0 = 0;
  int plays_1 = 0;
  int score = 0;

  for (int i = 0; i < NUM_PLAYS; i++) {
    switch (rand() % 6) {
      case 0:
        score = rand() % 1000;
        sprintf(msg[i], "add_player_score sam0 %d", score);
        plays_0++;
        if (score > high_score_0) {
          high_score_0 = score;
        }
        break;
      case 1:
        score = rand() % 1000;
        sprintf(msg[i], "add_player_score sam1 %d", score);
        plays_1++;
        if (score > high_score_1) {
          high_score_1 = score;
        }
        break;
      case 2:
        sprintf(msg[i], "get_total_plays");
        break;
      case 3:
        sprintf(msg[i], "get_best_player");
        break;
      case 4:
        sprintf(msg[i], "get_player_plays sam0");
        break;
      case 5:
        sprintf(msg[i], "get_num_players");
        break;
    }
    threads[i] = make_request_async(msg[i]);
  }
  for (int i = 0; i < NUM_PLAYS; i++) {
    pthread_join(*threads[i], NULL);
    printf(".");
    fflush(stdout);
  }
  printf("\n");

  char expected_response[100];

  sprintf(expected_response, "%d", plays_0);
  cr_assert_str_eq(
    (char*)make_request("get_player_plays sam0"),
    expected_response
  );
  sprintf(expected_response, "%d", plays_1);
  cr_assert_str_eq(
    (char*)make_request("get_player_plays sam1"),
    expected_response
  );

  cr_assert_str_eq(
    (char*)make_request("get_num_players"),
    "2"
  );

  int high_score = (high_score_0 > high_score_1) ? high_score_0 : high_score_1;
  sprintf(expected_response, "%d", high_score);
  cr_assert_str_eq(
    (char*)make_request("get_highest_score"),
    expected_response
  );

  sprintf(expected_response, "%d", plays_0 + plays_1);
  cr_assert_str_eq(
    (char*)make_request("get_total_plays"),
    expected_response
  );

  double time_end = currentTime();
  log_debug("run time: %f\n", (time_end - time_start));


  end_time = currentTime();
  printf("Time taken for mixed user workload: %f\n", end_time - start_time);

  pthread_cancel(server_thread);

  return 0;
}
