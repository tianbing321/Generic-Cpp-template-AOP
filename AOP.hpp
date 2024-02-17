#include <iostream>
#include <functional>


namespace aop
{
    template<typename T>
        struct A
        {
            T b;
        };
        template<>
        struct A<void>
        {
        };

	class NonCopyable
	{
	public:
		NonCopyable(const NonCopyable &) = delete;                // Deleted
		NonCopyable & operator = (const NonCopyable &) = delete;  // Deleted
		NonCopyable() = default;                                  // Available
	};

    #define HAS_MEMBER(member)                                                             \
                                                                                           \
    template<typename T, typename... Args> struct has_member_##member {	                   \
                                                                                           \
    private:                                                                               \
                                                                                           \
    	template<typename U> static auto Check(int)                                        \
    	-> decltype(std::declval<U>().member(std::declval<Args>()...), std::true_type());  \
                                                                                           \
    	template<typename U> static std::false_type Check(...);                            \
                                                                                           \
    public:                                                                                \
                                                                                           \
        enum { value = std::is_same<decltype(Check<T>(0)), std::true_type>::value };       }

	HAS_MEMBER(Before); // Add Before aspect
	HAS_MEMBER(After);  // Add After aspect
	HAS_MEMBER(push_back);  // Add After aspect

	template<typename Ret, typename Func, typename... Args>
	struct Aspect : NonCopyable
	{
        // using Ret = decltype(std::declval<Func>()(std::declval<Args>()...));

		Aspect(Func && f) : m_func(std::forward<Func>(f)) { }//构造函数 Fun f

		template<typename T>
		typename std::enable_if<
			has_member_Before<T, Args...>::value&&
			has_member_After<T, Args...>::value>::type
			Invoke(Args &&... args, T && aspect)
		{
			aspect.Before(std::forward<Args>(args)...);  // Run codes before core codes
            if constexpr(std::is_same_v<Ret, void>)
            {
                m_func(std::forward<Args>(args)...); // Run core codes
            }
            if constexpr(!std::is_same_v<Ret, void>)
            {
                m_ret.b = m_func(std::forward<Args>(args)...); // Run core codes
            }
            // else
			//     m_ret = m_func(std::forward<Args>(args)...); // Run core codes
			aspect.After(std::forward<Args>(args)...);   // Run codes after core codes
		}

		template<typename T>
		typename std::enable_if<
			has_member_Before<T, Args...>::value&&
			!has_member_After<T, Args...>::value>::type
			Invoke(Args &&... args, T && aspect)
		{
			aspect.Before(std::forward<Args>(args)...);  // Run codes before core codes
			// m_ret = m_func(std::forward<Args>(args)...); // Run core codes
		}

		template<typename T>
		typename std::enable_if<
			!has_member_Before<T, Args...>::value&&
			has_member_After<T, Args...>::value>::type
			Invoke(Args &&... args, T && aspect)
		{
			// m_ret = m_func(std::forward<Args>(args)...); // Run core codes
			aspect.After(std::forward<Args>(args)...);   // Run codes after core codes
		}

		template<typename Head, typename... Tail>
		void Invoke(Args &&... args, Head && headAspect, Tail &&... tailAspect)
		{
			headAspect.Before(std::forward<Args>(args)...);
			Invoke(std::forward<Args>(args)..., std::forward<Tail>(tailAspect)...);
			headAspect.After(std::forward<Args>(args)...);
		}

        template<class T = Ret, typename std::enable_if<std::is_void<T>::value, int>::type = 0>
		void GetReturn() { };
        template<class T = Ret, typename std::enable_if<!std::is_void<T>::value, int>::type = 0>
		Ret GetReturn() { return m_ret.b; };

	private:
		Func m_func;  // Function that be invoked
        A<Ret> m_ret;
	};

	template<typename T> using identity_t = T;

	// AOP function, for export
	template<typename Ret, typename... AP, typename... Args, typename Func>
    typename std::enable_if<std::is_same_v<Ret, void>, void>::type
	AOP(Func && f, Args &&... args)
	{
		Aspect<Ret, Func, Args...> asp(std::forward<Func>(f));
		asp.Invoke(std::forward<Args>(args)..., identity_t<AP>()...);
            return ;
	}

    template<typename Ret, typename... AP, typename... Args, typename Func>
    typename std::enable_if<!std::is_same_v<Ret, void>, Ret>::type
	AOP(Func && f, Args &&... args)
	{
		Aspect<Ret, Func, Args...> asp(std::forward<Func>(f));
		asp.Invoke(std::forward<Args>(args)..., identity_t<AP>()...);
        return asp.GetReturn();
	}

    // template<typename... AP, typename... Args, typename Func>
    template<typename... AP, typename... Args, typename Func ,typename TP = void,
    typename std::enable_if_t<!std::is_member_function_pointer_v<Func>,int > = 0 >
	auto AOPnew(Func && f, Args &&... args)//->decltype(auto)
	{
        std::cout << "start aop." << std::endl;
        using Ret = typename std::result_of<Func(Args...)>::type;//decltype(std::declval<Func>()(std::declval<Args>()...));//
		return (AOP<Ret, AP...>(std::forward<Func>(f), std::forward<Args>(args)...));
	}

    template<typename... AP, typename... Args, typename Func ,typename TP = void,
    typename std::enable_if<std::is_member_function_pointer<Func>::value, int>::type =0 >
    auto AOPnew(Func && f,TP* target, Args &&... args)//->decltype(auto)
    {
        std::cout << "start aop." << std::endl;
        auto bindfunc = [&](Args... args){ return (target->*std::forward<Func>(f))(std::forward<Args>(args)...);};
        // using Ret = typename std::result_of<Func(Args...)>::type;//decltype(std::declval<Func>()(std::declval<Args>()...));//
        // using Ret = typename std::result_of<(target->*std::forward<Func>(f))(std::forward<Args>(args)...)>::type;
        using Ret = typename std::invoke_result_t<Func,TP*, Args...>;//typename std::invoke_result<Func(Args...)>::type;
        // static_assert(std::is_same_v<Ret,std::string>,"------------------------Func must be a member function pointer.");
        return (AOP<Ret, AP...>(bindfunc, std::forward<Args>(args)...));
    }
}

using aop::AOP;

/* --- AOP user manual ---

  If you need to add aspects to a function and then call this function,
  you can call the function as follows:

    [func_return_value] AOP<[func_return_type], [aspect_struct...]>([func], [func_params...]);

  [func_return_value] : the original function return value
  [func_return_type]  : the original function return type, must not be "void" type
  [aspect_struct...]  : aspects list that will be added to original function
  [func]              : the original function pointer
  [func_params...]    : the original function params list

  Aspect struct needs to be coded as follows:

  struct [aspect_struct_name]
  {
      void Before(...)
      {
          // TODO
      }

      void After(...)
      {
          // TODO
      }

      [vars]

      private:

      [private_vars]
  };

------------------------*/
using namespace std;

struct AspectDemo
{
	void Before(...)
	{
		iCounter++;
		cout << "Aspect : Before, counter is " << iCounter << endl;
	}

	void After(...)
	{
		iCounter++;
		cout << "Aspect : After, counter is " << iCounter << endl;
	}

private:

	int iCounter = 0;
};

double Demoi(int i)
{
	cout << "Demoi run, input int value is "<< i << "." << endl;
	cout << "Demoi has a return value with type is double and value is -1.5." << endl;
	return -1.5;
}

bool Demob()
{
	cout << "Demob run, without input." << endl;
	cout << "Demob has a return value with type is bool and value is true." << endl;
	return true;
}

string Demostr()
{
	cout << "Demostr run, without input." << endl;
	cout << "Demostr has a return value with type is int and value is -1." << endl;
	return "123";
}
char* Demochar()
{
    cout << "Demostr run, without input." << endl;
	cout << "Demostr has a return value with type is int and value is -1." << endl;
	return "123";
}
void Demov()
{
	cout << "Demov run, without input." << endl;
	cout << "Demov has a return value with type is int and value is void." << endl;
	return ;
}
struct testA
{
    void testv_fun_i(int i)
    {
        cout << "testfun i:"<<i<<endl;
    }
    void test_vfun_stri(std::string& str,int i)
    {
        cout << "testfun str:"<<str<<endl;
        cout << "testfun i:"<<i<<endl;
    }
    string test_str_fun_i(int i)//string& str1, string& str2)
    {
        cout <<"struct  str_fun_i run i:"<<i<<endl;
        return "str1+str2";
    }
    string test_str_fun_strstr(string& str1, string& str2)
    {
        cout<<"testfun2 str1:"<<str1<<endl;
        cout<<"testfun2 str2:"<<str2<<endl;
        str2 = string("1234567890");
        return str1+str2;
    }
};
auto bindfun = [](auto&& classname,auto fun,auto&& ... args)
{
    return classname.fun(std::forward<decltype(args)>(args)...);
};
template <typename Ret, typename Aspect, typename Func, typename... Args>
typename std::enable_if<std::is_invocable<Func, Args...>::value, Ret>::type CallAOP(Func&& func, Args&&... args)
{
    return AOP<Ret, Aspect>(std::forward<Func>(func), std::forward<Args>(args)...);
}
#include <list>
#include <map>
#include <set>
#include <string>
#include <vector>
#include <type_traits>
int main()
{
    std::cout << aop::has_member_Before<AspectDemo >::value << std::endl;
    std::cout << "----------------------------" << std::endl;

	// cout << "AOP called : Demoi : " << endl;
	// cout << "Ret: " << (aop::AOPnew< AspectDemo>(Demoi, 12)) << endl;
	// cout << endl;

	// cout << "AOP called : Demob : " << endl;
	// cout << "Ret:" << (AOP<bool, AspectDemo, AspectDemo>(Demob)) << endl;
    // cout << endl;

    // cout << "Call directly : Demostr : " << endl;
    // cout << "Ret:" << (AOP<string, AspectDemo, AspectDemo>(Demostr)) << endl;
    // cout << endl;

    // cout << "AOP called : Demochar : " << endl;
    // cout << "Ret:" << (AOP<char*, AspectDemo, AspectDemo>(Demochar)) << endl;
    // cout << endl;

    // cout << "AOP called : Demov : " << endl;
    // (aop::AOPnew< AspectDemo, AspectDemo>(Demov));
    // cout << endl;

    // cout << "AOP called : structfun : " << endl;
    // testA classa;
    string str = "testfun";string str2 = "testfun2";
    // str.push_back(string("12"),);
    // // auto func = std::bind(&testA::test_str_fun_i, &classa, std::placeholders::_1);---ok
    // // cout << "Ret:" << (aop::AOP<string,AspectDemo>(func,11)) << endl;----ok
    // cout << "Ret:" << (aop::AOPnew<AspectDemo,AspectDemo>(&testA::test_str_fun_i, &classa,11)) << endl;
    // cout << "Ret:" << (aop::AOPnew<AspectDemo>(&testA::test_str_fun_strstr, &classa,str,str2)) << endl;
    // cout << "return fun str1:" << str << " str2:" << str2 << endl;
    // (aop::AOPnew<AspectDemo>(&testA::test_vfun_stri, &classa,str,11));
    // (aop::AOPnew<AspectDemo,AspectDemo>(&testA::testv_fun_i, &classa,11));
    // cout << endl;

    // system("pause");
	return 0;
}
