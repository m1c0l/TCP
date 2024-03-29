#include <sys/socket.h>
#include <sys/time.h>
#include <cstdlib>
#include <cstdio>
#include "Utils.h"
using namespace std;

// increment a seq or ack number and mod it by MAX_SEQ_NUM
uint16_t incSeqNum(uint16_t seq, uint16_t increment) {
	return (seq + increment) % MAX_SEQ_NUM;
}

// returns true if seq is within the range, counting for wraparound
bool inWindow(uint16_t seq, uint16_t bot, uint16_t top) {
	if (bot < top) { // no wraparound
		return seq >= bot && seq <= top;
	}
	else { // wraps around
		return seq <= top || seq >= bot;
	}
}

/* Subtract the ‘struct timeval’ values X and Y,
   storing the result in RESULT.
   Return 1 if the difference is negative, otherwise 0. */
int
timeval_subtract (struct timeval *result, struct timeval *x, struct timeval *y)
{
  /* Perform the carry for the later subtraction by updating y. */
  if (x->tv_usec < y->tv_usec) {
    int nsec = (y->tv_usec - x->tv_usec) / 1000000 + 1;
    y->tv_usec -= 1000000 * nsec;
    y->tv_sec += nsec;
  }
  if (x->tv_usec - y->tv_usec > 1000000) {
    int nsec = (x->tv_usec - y->tv_usec) / 1000000;
    y->tv_usec += 1000000 * nsec;
    y->tv_sec -= nsec;
  }

  /* Compute the time remaining to wait.
     tv_usec is certainly positive. */
  result->tv_sec = x->tv_sec - y->tv_sec;
  result->tv_usec = x->tv_usec - y->tv_usec;

  /* Return 1 if result is negative. */
  return x->tv_sec < y->tv_sec;
}

// sets a socket's receive timer to sec/usec
void setSocketTimeout(int sockfd, timeval tv) {
	int r = setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, (void*)&tv, sizeof(tv));
	if (r == -1) {
		perror("setsockopt");
		exit(1);
	}
}

// get the current time
timeval now() {
	timeval tv;
	gettimeofday(&tv, NULL);
	return tv;
}

// get the time from now until 0.5 seconds after start
timeval timeRemaining(timeval start) {
	timeval end = start;
	end.tv_usec += TIMEOUT * 1000;
	if (end.tv_usec >= 1000000) { // carry over
		end.tv_sec += 1;
		end.tv_usec -= 1000000;
	}
	timeval n = now();

	timeval result;
	int r = timeval_subtract(&result, &end, &n);

	// make result zero if negative
	if (r == 1) {
		result.tv_sec = 0;
		result.tv_usec = 0;
	}
	return result;
}

// returns true if a comes earlier than b
bool timeval_cmp(timeval a, timeval b) {
	if (a.tv_sec < b.tv_sec) {
		return true;
	}
	else if (a.tv_sec > b.tv_sec) {
		return false;
	}
	else {
		return a.tv_usec < b.tv_usec;
	}
}
