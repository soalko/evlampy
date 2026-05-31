#pragma once

#include <algorithm>
#include <vector>

template <typename Event>
class Observer {
public:
	virtual ~Observer() = default;
	virtual void onNotify(const Event& event) = 0;
};

template <typename Event>
class Subject {
public:
	void attach(Observer<Event>* observer) {
		observers_.push_back(observer);
	}

	void detach(Observer<Event>* observer) {
		observers_.erase(std::remove(observers_.begin(), observers_.end(), observer), observers_.end());
	}

protected:
	void notify(const Event& event) {
		for (auto* observer : observers_) {
			if (observer) {
				observer->onNotify(event);
			}
		}
	}

private:
	std::vector<Observer<Event>*> observers_;
};
