import sys
import time
import atexit

from .utils import eprint


__all__ = ['timer_start', 'timer_lap', 'timer_stop', 'timer_stop_all']
TIMERS = {}


def seconds_to_most_relevant_unit(s):
	s *= 1e6
	if s < 1000:
		return '{:.3f}µs'.format(s)

	s /= 1000
	if s < 1000:
		return '{:.3f}ms'.format(s)

	s /= 1000
	if s < 60:
		return '{:.3f}s'.format(s)

	s /= 60
	return '{:d}m {:.3f}s'.format(int(s), s/60%60)


def timer_start(name=sys.argv[0]):
	now_wall, now_cpu = time.monotonic(), time.process_time()
	TIMERS[name] = (now_wall, now_cpu, now_wall, now_cpu, 1)


def timer_lap(name=sys.argv[0]):
	now_wall, now_cpu =  time.monotonic(), time.process_time()
	*x, prev_wall, prev_cpu, lap = TIMERS[name]

	dt_wall = seconds_to_most_relevant_unit(now_wall - prev_wall)
	dt_cpu  = seconds_to_most_relevant_unit(now_cpu  - prev_cpu )

	eprint('Timer {} lap #{}: {} wall, {} CPU'.format(name, lap, dt_wall, dt_cpu))

	TIMERS[name] = (*x,  time.monotonic(), time.process_time(), lap + 1)


def timer_stop(name=sys.argv[0]):
	now_wall, now_cpu = time.monotonic(), time.process_time()
	prev_wall, prev_cpu, *_ = TIMERS.pop(name)

	dt_wall = seconds_to_most_relevant_unit(now_wall - prev_wall)
	dt_cpu  = seconds_to_most_relevant_unit(now_cpu  - prev_cpu )

	eprint('Timer {}: {} wall, {} CPU'.format(name, dt_wall, dt_cpu))


def timer_stop_all():
	now_wall, now_cpu = time.monotonic(), time.process_time()

	while TIMERS:
		k, v = TIMERS.popitem()
		prev_wall, prev_cpu, *_ = v
		dt_wall = seconds_to_most_relevant_unit(now_wall - prev_wall)
		dt_cpu  = seconds_to_most_relevant_unit(now_cpu  - prev_cpu )

		eprint('Timer {}: {} wall, {} CPU'.format(k, dt_wall, dt_cpu))


atexit.register(timer_stop_all)
