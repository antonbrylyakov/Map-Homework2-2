#pragma once
#include <functional>
#include <chrono>
#include <stdexcept>

using namespace std::chrono;
using TaskId = size_t;

// Информация о текущем прогрессе выполнения задания
struct ProgressInfo
{
	// ID задачи
	TaskId id;
	// ID потока
	std::thread::id threadId;
	// Размер задачи
	size_t total;
	// Текущий прогресс
	size_t processed;
	// Признак ошибки на текущем шаге
	bool error;
	std::string errorMessage;
	// Признак полного завершения задачи
	bool complete;
	// Время, прошедшее с начала выполнения задачи
	double elapsedSec;
};

template <typename T>
struct TaskStart
{
	TaskId id;
	size_t total;
	T param;
};

template <typename T>
using CallbackFunc = std::function<void(const TaskStart<T>&, const ProgressInfo&)>;

template <typename T>
class AbstractWorker
{
public:
	AbstractWorker(CallbackFunc<T> callback) : m_progressCallback(callback)
	{
	}
	
	void operator ()(const TaskStart<T> taskStart)
	{
		Run(taskStart);
	}

protected:
	CallbackFunc<T> m_progressCallback;

	void Run(const TaskStart<T>& taskStart)
	{
		ProgressInfo initialProgress
		{
			taskStart.id,
			std::this_thread::get_id(),
			taskStart.total,
			0,
			false,
			"",
			false,
			0
		};

		try
		{
			m_progressCallback(taskStart, initialProgress);
		}
		catch (...)
		{
		}

		auto start = steady_clock::now();
		for (auto step = 0; step < taskStart.total; ++step)
		{
			bool error = false;
			std::string errorMessage;
			try
			{
				RunTaskStep(taskStart, step);
			}
			catch (std::exception ex)
			{
				error = true;
				errorMessage = ex.what();
			}

			const auto now = steady_clock::now();
			const auto durationSeconds = static_cast<double>(duration_cast<milliseconds>(now - start).count() / 1000);
			ProgressInfo progress
			{
				taskStart.id,
				std::this_thread::get_id(),
				taskStart.total,
				step + 1,
				error,
				std::move(errorMessage),
				step == taskStart.total - 1,
				durationSeconds
			};

			try
			{
				m_progressCallback(taskStart, progress);
			}
			catch(...)
			{}
		}
	}

	virtual void RunTaskStep(const TaskStart<T>&, size_t step) = 0;
};

template<typename T>
using StepFunc = std::function<void(const TaskStart<T>&, size_t)>;

template <typename T>
class LambdaWorker: public AbstractWorker<T>
{
public:
	LambdaWorker(StepFunc<T> stepFunc, CallbackFunc<T> callback) : AbstractWorker<T>(callback), m_stepFunc(stepFunc)
	{
	}
protected:
	StepFunc<T> m_stepFunc;
	void RunTaskStep(const TaskStart<T>& taskStart, size_t step) override
	{
		m_stepFunc(taskStart, step);
	}
};