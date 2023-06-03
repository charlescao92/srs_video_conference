#ifndef KRTC_BASE_SINGLETON_H_
#define KRTC_BASE_SINGLETON_H_

template <typename T>
class Singleton {
public:
	static T& Instance() {
		static T instance;
		return instance;
	}

private:
	Singleton() {}
	~Singleton() {}
	Singleton(const Singleton&) = delete;
	Singleton& operator=(const Singleton&) = delete;
};

#endif // KRTC_BASE_SINGLETON_H_