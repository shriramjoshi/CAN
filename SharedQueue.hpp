#ifndef SHARED_QUEUE_HPP
#define SHARED_QUEUE_HPP
#include <queue>
#include <mutex>
#include <condition_variable>

template <typename T>
class SharedQueue
{
	public:
		SharedQueue() = default;
		~SharedQueue() = default;
		SharedQueue(const SharedQueue&) = delete;
		SharedQueue& operator==(const SharedQueue&) = delete;
		
		T& front()
		{
			std::unique_lock<std::mutex> mlock(mutex_);
			while (queue_.empty())
			{
				cond_.wait(mlock);
			}
			return queue_.front();
		}

		void pop_front()
		{
			std::unique_lock<std::mutex> mlock(mutex_);
			while (queue_.empty())
			{
				cond_.wait(mlock);
			}
			queue_.pop_front();
		}     

		void push_back(const T& item)
		{
			std::unique_lock<std::mutex> mlock(mutex_);
			queue_.push_back(item);
			mlock.unlock(); 
			cond_.notify_one();

		}

		void push_back(T&& item)
		{
			std::unique_lock<std::mutex> mlock(mutex_);
			queue_.push_back(std::move(item));
			mlock.unlock();     
			cond_.notify_one(); 

		}

		int size()
		{
			std::unique_lock<std::mutex> mlock(mutex_);
			int size = queue_.size();
			mlock.unlock();
			return size;
		}

		bool empty()
		{
			std::unique_lock<std::mutex> mlock(mutex_);
			bool isEmpty = queue_.empty();
			mlock.unlock();
			return isEmpty;
		}

	private:
		std::deque<T> queue_;
		std::mutex mutex_;
		std::condition_variable cond_;
};

#endif
