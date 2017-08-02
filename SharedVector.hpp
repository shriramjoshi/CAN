#ifndef SHARED_VECTOR_HPP
#define SHARED_VECTOR_HPP
#include <vector>
#include <mutex>
#include <condition_variable>

template <typename T>
class SharedVector
{
	using iterator = typename std::vector<T>::iterator;
	public:
		SharedVector() = default;
		~SharedVector() = default;
		SharedVector(const SharedVector&) = default;
		SharedVector& operator=(const SharedVector&) = default;
		//SharedVector(SharedVector&&) = default;
		//SharedVector& operator=(SharedVector&&) = default;
		
		void push_back(const T& item)
		{
			std::unique_lock<std::mutex> mlock(mutex_);
			vector_.push_back(item);
			mlock.unlock(); 
			cond_.notify_one();
		}

		void push_back(T&& item)
		{
			std::unique_lock<std::mutex> mlock(mutex_);
			vector_.push_back(std::move(item));
			mlock.unlock();     
			cond_.notify_one(); 
		}

		int size(void)
		{
			std::unique_lock<std::mutex> mlock(mutex_);
			int size = vector_.size();
			mlock.unlock();
			return size;
		}

		bool empty(void)
		{
			std::unique_lock<std::mutex> mlock(mutex_);
			bool isEmpty = empty();
			mlock.unlock();
			return isEmpty;
		}
		
		T& operator[](const int index)
		{
			std::lock_guard<std::mutex> mlock(mutex_);
			return vector_[index];
		}
		
		template<typename... Args>
		void emplace_back(Args... args)
		{
			std::unique_lock<std::mutex> mlock(mutex_);
			vector_.emplace_back(args...);
			mlock.unlock();
		}
		
		T& at(const int index)
		{
			std::lock_guard<std::mutex> mlock(mutex_);
			return vector_.at(index);
		}
		
		iterator begin(void) noexcept
		{
			std::lock_guard<std::mutex> mlock(mutex_);
			return vector_.begin();
		}
		
		iterator end(void) noexcept
		{
			std::lock_guard<std::mutex> mlock(mutex_);
			return vector_.end();
		}
		
		void swap(SharedVector<T>& obj1)
		{
			std::unique_lock<std::mutex> mlock(mutex_);
			using std::swap;
			swap(vector_, obj1.vector_);
			mlock.unlock();
		}
			
	private:
		std::vector<T> vector_;
		std::mutex mutex_;
		std::condition_variable cond_;
};

#endif
