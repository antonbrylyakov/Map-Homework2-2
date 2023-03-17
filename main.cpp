#include <iostream>
#ifdef _WIN32
#include "windows.h"
#endif

#include <chrono>
#include <random>
#include <thread>
#include <mutex>
#include "Worker.hpp"
#include "consol_parameter.hpp"

using namespace std::chrono;

// Данные запуска задачи
struct SimulationData
{
	// Время выполнения шага
	duration<double, std::milli> stepDelayMs;
	// Вероятность возникновения ошибки на шаге
	float errorProbability;
};


int main()
{
	setlocale(LC_ALL, "Russian");
#ifdef _WIN32
	SetConsoleCP(1251);
#endif

	// Инициализируем генератор случайных чисел для симуляции ошибок
	std::random_device rd;
	std::mt19937 gen(rd());
	std::uniform_real_distribution errorDist(0.0, 1.0);
	std::uniform_int_distribution taskSizeDist(100, 1000);

	// Функция выполнения шага задачи
	auto stepFunc = [&errorDist, &gen](const TaskStart<SimulationData>& taskStart, size_t step)
	{
		std::this_thread::sleep_for(taskStart.param.stepDelayMs);
		auto rnd = errorDist(gen);
		if (rnd < taskStart.param.errorProbability)
		{
			throw std::runtime_error("Произошла ошибка!");
		}
	};

	std::mutex mLock;
	std::vector<int> currentScreenPosition;

	// Функция вывода прогресса, специфическая для консоли
	auto callbackFunc = [&mLock, &currentScreenPosition](const TaskStart<SimulationData>& taskStart, const ProgressInfo& progress)
	{
		auto newPosition = static_cast<int>(80 * static_cast<float>(progress.processed) / progress.total);
		auto taskIndex = static_cast<size_t>(progress.id);
		auto currentPosition = currentScreenPosition[taskIndex];

		if (newPosition > currentPosition || progress.error || progress.complete)
		{
			currentScreenPosition[taskIndex] = newPosition;

			std::lock_guard<std::mutex> lkg(mLock);

			consol_parameter::SetColor(15, 0);
			auto infoYStart = progress.id * 5;
			consol_parameter::SetPosition(0, infoYStart);
			std::cout << "ID задачи: " << progress.id;

			consol_parameter::SetPosition(0, infoYStart + 1);
			std::cout << "ID потока: " << progress.threadId;

			consol_parameter::SetColor(15, 0);
			consol_parameter::SetPosition(0, infoYStart + 2);
			for (auto x = currentPosition + 1; x <= newPosition; ++x)
			{
				consol_parameter::SetPosition(x, infoYStart + 2);
				if (progress.error && x == newPosition)
				{
					// Выводим ошибку красным
					consol_parameter::SetColor(15, 12);
				}

				std::cout << "X";
			}

			if (progress.complete)
			{
				consol_parameter::SetPosition(0, infoYStart + 3);
				std::cout << "Затраченное время: " << progress.elapsedSec << "s";
			}
		}
	};

	consol_parameter::SetColor(15, 0);
	consol_parameter::clearScreen();

	size_t taskCount = 3;
	std::vector<std::thread> threads;

	// Создаем обработчик
	LambdaWorker<SimulationData> worker(stepFunc, callbackFunc);

	// Инициализируем задачи и запускаем в потоках
	for (size_t i = 0; i < taskCount; ++i)
	{
		currentScreenPosition.push_back(-1);
		auto taskSize = static_cast<size_t>(taskSizeDist(gen));
		// Веростность возникновения ошибки - 5%
		// Задачи с одинаковым временем выполнения шага, но рпзной длины
		threads.push_back(std::thread(worker, TaskStart<SimulationData>{ i, taskSize, { 10ms, 0.05 } }));
	}

	for (auto& t : threads)
	{
		t.join();
	}

	consol_parameter::SetColor(15, 0);
	consol_parameter::SetPosition(0, taskCount * 5 + 1);
	std::cout << "Расчет закончен" << std::endl;
}