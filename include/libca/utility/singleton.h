#ifndef LIBCA_UTILITY_SINGLETON_H
#define LIBCA_UTILITY_SINGLETON_H

#include <mutex>

namespace libca {
template <class T>
class Singleton
{
public:
	template <class... Args>
	static T* Instance(Args&&... args) {
		if (m_pInstance == nullptr) {
			std::lock_guard<std::mutex> lg(m_mutex);
			if (m_pInstance == nullptr)
				m_pInstance = new T(std::forward<Args>(args)...), m_autoRelease;
		}
		return m_pInstance;
	}

private:
	Singleton() = delete;
	virtual ~Singleton() = delete;
	Singleton(const Singleton&) = delete;
	Singleton& operator=(const Singleton&) = delete;

	class AutoRelease
	{
	public:
		AutoRelease() = default;

		~AutoRelease() {
			if (nullptr != m_pInstance) {
				delete m_pInstance;
				m_pInstance = nullptr;
			}
		}
	};
	static AutoRelease m_autoRelease;
	static T* m_pInstance;
	static std::mutex m_mutex;
};

template <class T>
T* Singleton<T>::m_pInstance = nullptr;

template <class T>
typename Singleton<T>::AutoRelease Singleton<T>::m_autoRelease;

template <class T>
std::mutex Singleton<T>::m_mutex;
} // namespace libca

#endif /* !LIBCA_UTILITY_SINGLETON_H */
