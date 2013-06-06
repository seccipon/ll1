#include <iostream>
#include <typeinfo>
#include <boost/shared_ptr.hpp>
#include <boost/bind.hpp>
#include <boost/thread.hpp>
#include <queue>
using namespace std;

class IEventHandler;


class IEvent {
public:
  virtual void PostToHandler(IEventHandler * eventHandler) const = 0;
  virtual std::string GetExplanation() const {
    return "no";
  }
};

class CEventUnhandledException : public IEvent
{
  std::string m_exceptionMessage;
public:
  CEventUnhandledException(const std::string & exceptionMessage) :
    m_exceptionMessage(exceptionMessage)
  {}

  const std::string & GetExceptionMessage() const {
    return m_exceptionMessage;
  }

  std::string GetExplanation() const {
    return std::string("Unexpected exception : ") + m_exceptionMessage;
  }

  virtual void PostToHandler(IEventHandler * eventHandler) const;
};


class CEventReadCompleted : public IEvent
{
public :
  virtual void PostToHandler(IEventHandler * eventHandler) const;
  virtual std::string GetExplanation() const {
    return "Read success!";
  }
};

class CEvent1 : public IEvent {
public:
  virtual void PostToHandler(IEventHandler * eventHandler) const;
  virtual std::string GetExplanation() const {
    return "event1";
  }
};

class CEventSocketError : public IEvent
{
public :
  virtual void PostToHandler(IEventHandler *eventHandler) const;
  virtual std::string GetExplanation() const {
    return "socket error";
  }
};

typedef boost::shared_ptr<IEvent> PEvent;

////////////////////////////////////////////////////////////////
class IEventHandler
{
public:

  // All-task-wide handlers are here (default handler should by present anyway)
  // default handler for unexpected exception
  virtual void HandleEvent(const CEventUnhandledException &event) {
    DefaultHandlerUnhandledException(event);
  }


  //task-specific handlers, filled by default handler
  virtual void HandleEvent(const CEventReadCompleted & event) {
    UnexpectedEvent(&event);
  }

  virtual void HandleEvent(const CEventSocketError & event) {
    UnexpectedEvent(&event);
  }

  virtual void HandleEvent(const CEvent1 & event) {
    UnexpectedEvent(&event);
  }

  //handle event by base-class pointer
  void HandleEvent(const PEvent & event) {
    event->PostToHandler(this);
  }

private:

  //unexpected event handler. Can be overloaded.
  virtual void UnexpectedEvent(const IEvent * event) const {
    cout << "Unexpected Event!" << event->GetExplanation() << endl;
  }


  //default handler for unexpected exception event
  void DefaultHandlerUnhandledException(const CEventUnhandledException & event) const {
    cout << "UNHANDLED exception : " << event.GetExceptionMessage() << endl;
  }
};

class CEventHandlerNull : public IEventHandler
{
private:
  virtual void UnexpectedEvent(const IEvent */*event*/) const {
    //do nothing
  }
};

class CEventHandlerReadTask : public IEventHandler
{
public:
  virtual void HandleEvent(const CEventReadCompleted &event) {
    cout <<event.GetExplanation() << endl;
  }

  virtual void HandleEvent(const CEventSocketError & event) {
    cout <<event.GetExplanation() << endl;
  }
};

class CEventHandlerSomthing : public IEventHandler
{

};
typedef boost::shared_ptr<IEventHandler> PEventHandler;

#define EVENT_POST_TO_HANDLER_IMPL(EventClassName) \
void EventClassName::PostToHandler(IEventHandler *eventHandler) const \
{ \
  eventHandler->HandleEvent(*this); \
}


EVENT_POST_TO_HANDLER_IMPL(CEventUnhandledException)
EVENT_POST_TO_HANDLER_IMPL(CEventReadCompleted)
EVENT_POST_TO_HANDLER_IMPL(CEventSocketError)
EVENT_POST_TO_HANDLER_IMPL(CEvent1)
////////////////////////////////////////////////////////////////


class IRunnable {
public:
  virtual void Run() = 0;
};



class CTask : public IRunnable {
  PEventHandler m_eventHandler;

protected:
  PEventHandler GetEventHandler() {
    return m_eventHandler;
  }

public:

  CTask(PEventHandler eventHandler) :
    m_eventHandler(eventHandler) {  }

  virtual void RunTask() = 0;
  virtual void Run();
};



typedef boost::shared_ptr<CTask> PTask;

typedef boost::shared_ptr<IRunnable> PRunnable;


template <typename T> class CSharedQueue
{
  std::deque<T> m_queue;
  boost::mutex m_mutex;
  boost::condition_variable m_condVar;
public:
  void Put(const T& t)
  {
    boost::unique_lock<boost::mutex> lock(m_mutex);
    m_queue.push_back(t);
    m_condVar.notify_one();
  }

  bool GetNonblock(T & t)
  {
    boost::unique_lock<boost::mutex> lock(m_mutex);
    if (m_queue.empty()) {
      return false;
    } else {
      t = m_queue.front();
      m_queue.pop_front();
      return true;
    }
  }

  bool GetWaitblock(T & t)
  {
    boost::unique_lock<boost::mutex> lock(m_mutex);


    if (m_queue.empty()) {
      m_condVar.timed_wait(lock, boost::get_system_time() + boost::posix_time::seconds(1));
    }

    if (!m_queue.empty())  {
      t = m_queue.front();
      m_queue.pop_front();
      return true;
    } else {
      return false;
    }
  }

  bool IsEmpty()
  {
    boost::unique_lock<boost::mutex> lock(m_mutex);
    return m_queue.empty();

  }
};

class CThreadPool {
  typedef boost::shared_ptr<boost::thread> PThread;
  size_t m_threadsCnt;

  std::vector<PThread> m_threads;
  CSharedQueue<PRunnable> m_queue;
  bool m_cancel;
public:

  CThreadPool(size_t threadsCnt) :
    m_threadsCnt(threadsCnt),
    m_cancel(false)
  {}

  void Run() {
    m_threads.reserve(m_threadsCnt);
    for (size_t i = 0; i < m_threadsCnt; i++) {
      m_threads.push_back(PThread(new boost::thread(boost::bind(&CThreadPool::ThreadRun, this))));
    }
  }

  void ThreadRun() {
    cerr << "thread up" << endl;
    for (;;) {
      PRunnable p;
      if (m_queue.GetWaitblock(p)) {
        //NULL ptr means wakeup
        if (p) {
          try {
            p->Run();
          } catch(...) {
            cerr << "unexpected exception in thread pool!";
            assert(false);
          }
        } else if (m_cancel && m_queue.IsEmpty()) {
          cerr <<"TD" << endl;
          break;
        }
      }
    }
    cerr << "thread down" << endl;
  }


  void Cancel() {
    m_cancel = true;
  }

  void Join() {
    for_each(m_threads.begin(), m_threads.end(), boost::bind(&boost::thread::join, _1));
  }

  void PostRunnable(PRunnable runnable) {
    cerr << "PR" << endl;
    m_queue.Put(runnable);
  }

  void BroadcastFastCancell()
  {
    for (size_t i = 0; i < m_threadsCnt; i++) {
      m_queue.Put(PRunnable());
      cerr << "cancel broad" << endl;
    }
  }
};

typedef boost::shared_ptr<CThreadPool> PThreadPool;

PThreadPool tp;



class CTaskHandleEvent : public CTask
{
  PEventHandler m_handler;
  PEvent m_event;
public:
  CTaskHandleEvent(PEventHandler handler, PEvent event) :
    CTask(PEventHandler(new CEventHandlerNull)),
    m_handler(handler),
    m_event(event)

  {}


  virtual void RunTask() {
//    CEventHandlerTaskWrapper::HandleYield(m_handler, m_event);
    m_handler->HandleEvent(m_event);
  }
};

class CEventHandlerTaskWrapper
{
public:
  static void HandleYield(PEventHandler handler, PEvent event) {
    handler->HandleEvent(event);
  }

  static void HandleDetached(PEventHandler handler, PEvent event, PThreadPool threadPool) {
    threadPool->PostRunnable(PRunnable(new CTaskHandleEvent(handler, event)));
  }
};


void CTask::Run() {
  try {
    cerr << "TASK RUN" << endl;
    RunTask();

  } catch(...) {
    cerr << "TASK STOP" <<endl;
    CEventHandlerTaskWrapper::HandleYield(m_eventHandler, PEvent(new CEventUnhandledException("unknown exception")));
    return;
  }
  cerr << "TASK STOP" <<endl;
}





class CTaskDummy : public CTask
{
public:
  CTaskDummy(PEventHandler eventHandler) :
    CTask(eventHandler)
  {

  }

  virtual void RunTask() {
    std::cout <<  "i am here fuck you" << endl;
    PEvent event (new CEvent1);
    CEventHandlerTaskWrapper::HandleDetached(GetEventHandler(), event, tp);
  }
};


typedef boost::shared_ptr<CTaskDummy> PTaskDummy;

int main()
{
  PEventHandler handler (new CEventHandlerReadTask);
//  PEvent rCompleted(new CEventReadCompleted);
//  PEvent sError(new CEventSocketError);
//  PEvent uEx(new CEventUnhandledException("peace death"));
//  PEvent ev1(new CEvent1);
//  handler->HandleEvent(rCompleted);
//  handler->HandleEvent(sError);
//  handler->HandleEvent(uEx);
//  handler->HandleEvent(ev1);

  tp = PThreadPool(new CThreadPool(5));
  tp->Run();



  PTaskDummy dummyTask (new CTaskDummy(handler));
  tp->PostRunnable(dummyTask);
//  sleep(1);
  tp->Cancel();
  tp->BroadcastFastCancell();
  tp->Join();

  return 0;
}

//#include <iostream>

//using namespace std;



//class B1;

//class A1
//{
//public:
//  virtual void DoThing() = 0;
//  virtual void HandleB(B1 * p);
//};


//class A2 : public A1
//{
//public:
//  virtual void DoThing() {}
//  virtual void HandleB(B1 *p);
//};

//class A3 : public A1
//{
//public:
//  virtual void DoThing() {}
//  virtual void HandleB(B1 *p);
//};

//class A4 : public A1
//{
//public :
//  virtual void DoThing() {}
//  virtual void HandleB(B1 *p);
//};

//class B1
//{
//public:
//  virtual void HandleA(const A1 & a);//by default

//  virtual void HandleA(const A2 & a);
//  virtual void HandleA(const A3 & a);


//};


//class B2 : public B1
//{
//  virtual void HandleA(const A2 & a);
//  virtual void HandleA(const A3 & a);
//};


//void A1::HandleB(B1 *p)
//{
//  p->HandleA(*this);
//}

//void A2::HandleB(B1 *p)
//{
//  p->HandleA(*this);
//}

//void A3::HandleB(B1 *p)
//{
//  p->HandleA(*this);
//}

//void A4::HandleB(B1 *p) {
//  p->HandleA(*this);
//}

//void B1::HandleA(const A1 & a)
//{
//  cout << "B1:Handle A1" << endl;
//}

//void B1::HandleA(const A2 & a)
//{
//  cout << "B1:Handle A2" << endl;
//}

//void B1::HandleA(const A3 & a)
//{
//  cout << "B1:Handle A3" << endl;
//}

//void B2::HandleA(const A2 & a)
//{
//  cout << "B2:Handle A2" << endl;
//}

//void B2::HandleA(const A3 & a)
//{
//  cout << "B2:Handle A3" << endl;
//}

//int main()
//{
//  A1 * a2 = new A2;
//  A1 * a3 = new A3;
//  A1 * a4 = new A4;
//  B1 * b1 = new B1;
//  B1 * b2 = new B2;

//  a2->HandleB(b1);
//  a2->HandleB(b2);
//  a3->HandleB(b1);
//  a3->HandleB(b2);
//  a4->HandleB(b1);

//  return 0;
//}

