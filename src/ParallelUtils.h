#include <thread>

#include <vector>

#include <mutex>

#include <condition_variable>

#include <functional>

#include <atomic>

class ThreadPool
{
public:
	typedef std::function<void(void)> Job;

	ThreadPool(int poolSize)
	{
		for (int threadIndex = 0; threadIndex < poolSize; ++threadIndex)
		{
			pool.push_back(std::thread(&ThreadPool::AwaitTask, this));
		}
	}

	~ThreadPool()
	{
		{
			

			std::unique_lock<std::mutex> destructionLock(destructionMutex);

			if (state == PoolState::Stopped)
			{
				return;
			}

			if (state == PoolState::Stopping)
			{
				abort();
				return;
			}
		}

		Stop();
	}

	void AddJob(Job jobToAdd)
	{
		{	
			std::unique_lock<std::mutex> queueLock(jobQueueMutex);

			pendingJobs.push_back(jobToAdd);
		}

		jobCondition.notify_one();
	}

	void Stop()
	{
		{
			PoolState expectedStateBeforeStop = PoolState::Running;

			volatile bool isAlreadyStopping = !state.compare_exchange_strong(expectedStateBeforeStop, PoolState::Stopping);

			if (isAlreadyStopping)
			{
				return;
			}
		}

		{
			std::unique_lock<std::mutex> jobQueueLock(jobQueueMutex);

			jobCondition.notify_all();
		}

		for (std::thread& poolThread : pool)
		{
			poolThread.join();
		}

		pool.clear();

		{
			PoolState expectedStateAfterStop = PoolState::Stopping;

			volatile bool isAlreadyStopped = !state.compare_exchange_strong(expectedStateAfterStop, PoolState::Stopped);

			if (isAlreadyStopped)
			{
				abort();
			}
		}
	}

private:
	void AwaitTask()
	{
		while (true)
		{
			Job jobToDo;
			{
				std::unique_lock<std::mutex> jobQueueLock(jobQueueMutex);

				jobCondition.wait(jobQueueLock, [this]() { return state != PoolState::Running || !pendingJobs.empty(); });

				if (state != PoolState::Running && pendingJobs.empty())
				{
					return;
				}

				jobToDo = *pendingJobs.rbegin();
				pendingJobs.pop_back();
			}

			if (jobToDo)
			{
				jobToDo();
			}
			else
			{
				abort();
			}
		}
	}

	enum class PoolState
	{
		Running,
		Stopping,
		Stopped,
	};

	std::atomic<PoolState> state = PoolState::Running;

	std::mutex jobQueueMutex;

	std::mutex destructionMutex;


	std::vector<Job> pendingJobs;

	std::vector<std::thread> pool;

	std::condition_variable jobCondition;
};

template <class StageLogic>
struct ParallelStage
{
	typedef StageLogic TStage;

	ParallelStage(const TStage& inStage, ThreadPool& inPool) : stage(inStage), workStrategy(WorkStrategy::Threadpool), pool(&inPool)
	{
		
	}

	ParallelStage(const TStage& inStage, bool inUseThread = false) : stage(inStage), workStrategy(inUseThread ? WorkStrategy::CreateAndJoinThread : WorkStrategy::Threadless)
	{

	}

	ParallelStage() = default;
	~ParallelStage() = default;
	ParallelStage(const ParallelStage&) = delete;
	ParallelStage& operator=(const ParallelStage&) = delete;
	ParallelStage(ParallelStage&&) = default;
	ParallelStage& operator=(ParallelStage&&) = default;

	void StartWork()
	{
		switch (workStrategy)
		{
		case WorkStrategy::Threadless:
			DoWork();
			break;
		case WorkStrategy::CreateAndJoinThread:
			stageThread = std::thread(&ParallelStage<StageLogic>::DoWork, this);
			break;
		case WorkStrategy::Threadpool:
			{
				ThreadPool::Job job = [this]()
				{
					DoWork();

					{
						std::unique_lock<std::mutex> workCompletionLock(workerMutex);
						bIsWorkComplete = true;
					}

					workCompletionNotificator.notify_one();
				};
				pool->AddJob(job);
				break;
			}
		default:
			break;
		}
	}

	void DoWork()
	{
		stage.DoWork();
	}

	void FinishWork()
	{
		switch (workStrategy)
		{
		case WorkStrategy::CreateAndJoinThread:
			stageThread.join();
			stageThread = std::thread();
			break;

		case WorkStrategy::Threadpool:
			{
				std::unique_lock<std::mutex> workCompletionLock(workerMutex);

				workCompletionNotificator.wait(workCompletionLock, [this]() { return bIsWorkComplete; });

				pool = nullptr;

				break;
			}
		default:
			break;
		}
	}

	enum class WorkStrategy
	{
		Threadless,
		CreateAndJoinThread,
		Threadpool,
	};

	TStage stage;
	WorkStrategy workStrategy = WorkStrategy::Threadless;
	
	std::thread stageThread;
	
	ThreadPool* pool = nullptr;
	bool bIsWorkComplete = false;
	std::mutex workerMutex;
	std::condition_variable workCompletionNotificator;
};

template <class StageLogic>
std::vector<StageLogic> RunStage(std::function<StageLogic(int)> allocator, int stagesCount, bool bUseThreads = false)
{
	std::vector<std::unique_ptr<ParallelStage<StageLogic>>> stages;

	stages.reserve(stagesCount);

	for (int stageIndex = 0; stageIndex < stagesCount; ++stageIndex)
	{
		stages.emplace_back(std::make_unique<ParallelStage<StageLogic>>(allocator(stageIndex), bUseThreads));
		
		(*stages.rbegin())->StartWork();
	}

	for (int stageIndex = 0; stageIndex < stagesCount; ++stageIndex)
	{
		stages[stageIndex]->FinishWork();
	}

	std::vector<StageLogic> stageLogic;

	for (std::unique_ptr<ParallelStage<StageLogic>>& stage : stages)
	{
		stageLogic.push_back(stage->stage);
	}

	return stageLogic;
}

template <class StageLogic>
std::vector<StageLogic> RunStage(std::function<StageLogic(int)> allocator, int stagesCount, ThreadPool& pool)
{
	std::vector<std::unique_ptr<ParallelStage<StageLogic>>> stages;

	stages.reserve(stagesCount);

	for (int stageIndex = 0; stageIndex < stagesCount; ++stageIndex)
	{
		stages.emplace_back(std::make_unique<ParallelStage<StageLogic>>(allocator(stageIndex), pool));

		(*stages.rbegin())->StartWork();
	}

	for (int stageIndex = 0; stageIndex < stagesCount; ++stageIndex)
	{
		stages[stageIndex]->FinishWork();
	}

	std::vector<StageLogic> stageLogic;

	for (std::unique_ptr<ParallelStage<StageLogic>>& stage : stages)
	{
		stageLogic.push_back(stage->stage);
	}

	return stageLogic;
}

