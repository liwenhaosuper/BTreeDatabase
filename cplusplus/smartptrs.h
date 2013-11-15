

#if !defined(__smartptrs_h)
#define __smartptrs_h

namespace Database
{
	/*!
	Base class of smart pointer classes.
	*/
	class RefCount
	{
	private:
		int _crefs;

	public:
		RefCount() : _crefs(0) {}
		virtual ~RefCount() {}
		virtual void upcount() { ++_crefs; }
		virtual void downcount(void)
		{
			if (--_crefs == 0)
			{
				delete this;
			}
		}
		virtual bool lessthan(const RefCount* other)
		{
			return this < other;
		}
	};

	/*!
	Smart pointer template.
	*/
	template <class T> class Ptr
	{
	private:
		T* _p;

	public:
		Ptr(const Ptr<T>& ptr) : _p(ptr._p) { if (_p) _p->upcount(); }
		Ptr() : _p(0) {}
		Ptr(T* p) : _p(p) { if (_p) _p->upcount(); }
		~Ptr(void) { if (_p) _p->downcount(); }
		operator T*(void) const { return _p; }
		T& operator*(void) const { return *_p; }
		T* operator->(void) const { return _p; }
		Ptr& operator=(const Ptr<T> &p) { return operator=((T*)(Ptr<T>)p); }
		Ptr& operator=(T* p)
		{
			if (_p) _p->downcount();
			_p = p;
			if (_p) _p->upcount();
			return *this;
		}
		Ptr& operator=(const T* p)
		{
			return operator=((T*)p);
		}
		bool operator ==(const Ptr<T> &p) const { return _p == p._p; }
		bool operator ==(const T* p) const { return _p == p; }
		bool operator !=(const Ptr<T> &p) const { return _p != p._p; }
		bool operator !=(const T* p) const { return _p != p; }
		bool operator < (const Ptr<T> &p) const { return _p->lessthan(p._p); }
	};

	typedef Ptr<RefCount> RefCountPtr;
};

#endif

