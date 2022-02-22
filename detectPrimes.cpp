// File: detectingPrimes.cpp
// Author: Alexander Varga
// date: 16/11/2021
#include "detectPrimes.h"
#include <iostream>
#include <vector>
#include <cmath>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <atomic>
//No thread cancellation

//atomic bool variable to hold the state of whether all the nums read from the text file have been parsed for primality
std::atomic<bool> glbl_finished;
//global bool for setting index'd nums primality
//portable for all architectures in contrast to atomic_bool
std::atomic<bool> glbl_prime_num;
//global vector of nums to be parsed for primality - shared amongst all threads
std::vector<int64_t> glbl_nums;
//global vector to store nums that are prime
std::vector<int64_t> glbl_result;
//global int to keep track of glbl_nums accessed in thread function
int glbl_idx = 0;
//global int number of threads being launched - shared amongst all threads
int64_t glbl_n_threads;
//global int to store numb being checked for primality
int64_t glbl_cursor;
//global ints to store divided workloads amongst threads
//lower bound
int64_t glbl_l;
//upper bound
int64_t glbl_u;
//middle for multithreading
int64_t glbl_mid;
//start bound for divided workload 
int64_t glbl_s;
//end bound for divided workload 
int64_t glbl_e;

class simple_barrier 
{
	std::mutex m_;
	std::condition_variable cv_;
	int n_remaining_, count_;
	bool coin_;
	public:
		simple_barrier(int count) 
		{
		count_ = count;
		n_remaining_ = count_;
		coin_ = false;
		}
		bool wait() {
		std::unique_lock<std::mutex> lk(m_);
		if( n_remaining_ == 1) {
		  coin_ = ! coin_;
		  n_remaining_ = count_;
		  cv_.notify_all();
		  return true;
		}
		auto old_coin = coin_;
		n_remaining_ --;
		cv_.wait(lk, [&](){ return old_coin != coin_; });
		return false;
		}
};
	
// returns true if n is prime, otherwise returns false
static bool is_prime(int64_t n)
{
  // handle trivial cases
  if (n < 2) return false;
  if (n <= 3) return true; // 2 and 3 are primes
  if (n % 2 == 0) return false; // handle multiples of 2
  if (n % 3 == 0) return false; // handle multiples of 3
  // try to divide n by every number 5 .. sqrt(n)
  int64_t i = 5;
  int64_t max = sqrt(n);
  while (i <= max) {
    if (n % i == 0) return false;
    if (n % (i + 2) == 0) return false;
    i += 6;
  }
  // didn't find any divisors, so it must be a prime
  return true;
}

void serial_task1()
{
	//get next number from glbl_nums
	glbl_cursor = glbl_nums[glbl_idx];
	glbl_prime_num.store(true, std::memory_order_relaxed);
	//Check if all nums have been checked for primality
	if((int)glbl_nums.size() <= glbl_idx)
	{
		//If so, set glbl_finished flag true to indicate to all threads to quit
		glbl_finished.store(true,std::memory_order_relaxed);
	}
	//Otherwise, divide workload for each thread
	//follows similar steps to the is_prime workload division 
	//perform trivial checks on num, store result in atomic bool
	else 
	{
		glbl_l = 5;
		glbl_u = sqrt(glbl_cursor);
		glbl_mid = std::ceil(double(glbl_u-glbl_l)/glbl_n_threads);
		//check trivial cases
		if (glbl_cursor < 2) glbl_prime_num.store(false, std::memory_order_relaxed);
		if (glbl_cursor <= 3) glbl_prime_num.store(true, std::memory_order_relaxed); // 2 and 3 are primes
		if (glbl_cursor % 2 == 0) glbl_prime_num.store(false, std::memory_order_relaxed); // handle multiples of 2
		if (glbl_cursor % 3 == 0) glbl_prime_num.store(false, std::memory_order_relaxed); // handle multiples of 3
		//check if nums is a square 
		if(sqrt(glbl_cursor) == glbl_u || glbl_cursor % glbl_u == 0 || glbl_cursor % (glbl_u+2) == 0 )
		{
			glbl_prime_num.store(false, std::memory_order_relaxed);
		}
		//split mid, determine if small number
		if(glbl_mid > 6)
		{
			glbl_mid = glbl_mid - (glbl_mid%6) - 6;
		} 
		else if ( glbl_mid <= 6)
		{
			glbl_prime_num.store(is_prime(glbl_cursor), std::memory_order_relaxed);
		} 
		//update index
		glbl_idx++;
	}
}

void parallel_task()
{
	int64_t i = glbl_s;
	//parallelize checking primality - essentially a multithreaded version of the code in the is_prime function
	while(i <= glbl_e)
	{
		//if num not prime, break from loop
		if(!glbl_prime_num.load())
		{
			break;
		}
		if(glbl_cursor % i == 0)
		{
			glbl_prime_num.store(false, std::memory_order_relaxed);
			break;
		}
		if(glbl_cursor % (i+2) == 0)
		{
			glbl_prime_num.store(false, std::memory_order_relaxed);
			break;
		}
		i+=6;
	}
}

void serial_task2()
{
	//if stored num is prime, combine the per-thread results 
	if(glbl_prime_num.load())
	{
		glbl_result.push_back(glbl_cursor);
	}	
}

//Thread function that will parallelize work and use barriers to serialize work when needed
void thread_function(int64_t tid, simple_barrier & barrier)
{
	//Repeat forever
	while(1)
	{ 
		//serial task 1 - pick one thread using barrier
		if(barrier.wait())
		{
			serial_task1();
		}
		//parallel task - executed by all threads, via barrier
		//if glbl_finished flag is set, exit threads i.e loop
		barrier.wait();
		if(glbl_finished.load())
		{
			break;
		}
		//otherwise, do the work assigned above split up in serial task1 and record per-thread result
		else
		{
			//initialize start and end bound for parallelization of workloads
			glbl_s = glbl_l + glbl_mid * (long)tid;
			glbl_e = glbl_s + glbl_mid;
			//start cannot be greater than upper bound nor less than lower bound
			//end cannot be greater than upper bound
			if(glbl_s > glbl_u) glbl_s = glbl_u;
			if(glbl_s < glbl_l) glbl_s = glbl_l;
			if(glbl_e > glbl_u) glbl_e = glbl_u;
			glbl_e = glbl_e/glbl_n_threads;
			parallel_task();
		}
		//serial task 2 - pick one thread using barrier
		if(barrier.wait())
		{
			serial_task2();
		}	
		barrier.wait();
	}
}
	
// This function takes a list of numbers in nums[] and returns only numbers that
// are primes.
//
// The parameter n_threads indicates how many threads should be created to speed
// up the computation.
std::vector<int64_t>
detect_primes(const std::vector<int64_t> & nums, int n_threads)
{
  //If only one thread is desired then no parallelization needs to be done
  if(n_threads == 1)
  {
	for (auto num : nums) 
	{
		if (is_prime(num)) glbl_result.push_back(num);
	}
  } 
  //Multiple threads - must split up the vector of nums amongst n_threads
  else
  {
	  //Prepare memory for threads
	  // 	- Initialize global vector of nums to be shared between the threads
	  //	- Intialize vector of type thread
	  //	- Intialize barrier object for n_threads: from Pavol's barrier implementation
	  // 	- Initalize glbl_finished atomic flag as false
	  glbl_nums = nums;
	  glbl_n_threads = n_threads;
	  std::vector<std::thread> threads;
	  simple_barrier barrier(n_threads);
	  glbl_finished.store(false, std::memory_order_relaxed);
	  //Launch threads
	  for(int i = 0; i < n_threads; i++)
	  {
		  threads.push_back(std::thread(thread_function, i, std::ref(barrier)));
	  }
	  //Join threads
	  for(auto && t: threads)
	  {
		  t.join();
	  }
  }  
  return glbl_result;
}