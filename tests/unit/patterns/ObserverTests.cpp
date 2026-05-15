#include "patterns/Observer.h"

#include <gtest/gtest.h>

#include <vector>

namespace {
class IntObserver : public Observer<int> {
public:
    void onNotify(const int& event) override {
        received.push_back(event);
    }

    std::vector<int> received;
};

class IntSubject : public Subject<int> {
public:
    void publish(int e) {
        notify(e);
    }
};
}

TEST(ObserverTests, AttachNotifiesObserver) {
    IntSubject subject;
    IntObserver observer;
    subject.attach(&observer);

    subject.publish(42);
    ASSERT_EQ(observer.received.size(), 1u);
    EXPECT_EQ(observer.received[0], 42);
}

TEST(ObserverTests, DetachStopsNotifications) {
    IntSubject subject;
    IntObserver observer;
    subject.attach(&observer);
    subject.detach(&observer);

    subject.publish(42);
    EXPECT_TRUE(observer.received.empty());
}

TEST(ObserverTests, MultipleObserversReceiveEvent) {
    IntSubject subject;
    IntObserver a;
    IntObserver b;
    subject.attach(&a);
    subject.attach(&b);

    subject.publish(7);
    ASSERT_EQ(a.received.size(), 1u);
    ASSERT_EQ(b.received.size(), 1u);
    EXPECT_EQ(a.received[0], 7);
    EXPECT_EQ(b.received[0], 7);
}

TEST(ObserverTests, DuplicateAttachProducesDuplicateNotification) {
    IntSubject subject;
    IntObserver observer;
    subject.attach(&observer);
    subject.attach(&observer);

    subject.publish(5);
    EXPECT_EQ(observer.received.size(), 2u);
}

TEST(ObserverTests, NullObserverIgnored) {
    IntSubject subject;
    IntObserver observer;
    subject.attach(nullptr);
    subject.attach(&observer);

    subject.publish(9);
    ASSERT_EQ(observer.received.size(), 1u);
    EXPECT_EQ(observer.received.front(), 9);
}

TEST(ObserverTests, DetachUnknownObserverHasNoEffect) {
    IntSubject subject;
    IntObserver attached;
    IntObserver unknown;
    subject.attach(&attached);

    subject.detach(&unknown);
    subject.publish(1);

    EXPECT_EQ(attached.received.size(), 1u);
}

TEST(ObserverTests, NotificationOrderMatchesAttachOrder) {
    IntSubject subject;
    IntObserver first;
    IntObserver second;
    subject.attach(&first);
    subject.attach(&second);

    subject.publish(100);

    ASSERT_EQ(first.received.size(), 1u);
    ASSERT_EQ(second.received.size(), 1u);
    EXPECT_EQ(first.received[0], 100);
    EXPECT_EQ(second.received[0], 100);
}

