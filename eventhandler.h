//#ifndef EVENTHANDLER_H
//#define EVENTHANDLER_H
//#include "event.h"
//#include "event1.h"
//class IEventHandler
//{
//public:
//  //event handler by default, failback for incompatible events
//  void HandleEvent(const IEvent & event);


//  // All-task-wide handlers
//  //default handler for unexpected exception
//  virtual void HandleEvent(const CEventUnhandledException &event);


//  //task-specific handlers
//  virtual void HandleEvent(const CEventReadCompleted & event) = 0;


//  //handle event by base-class pointer
//  void HandleEvent(IEvent * event) {
//    event->PostToHandler(this);
//  }
//};


//class CEventHandlerReadTask : public IEventHandler
//{
//public:
//  virtual void HandleEvent(const CEventReadCompleted &event);
//};

//#endif // EVENTHANDLER_H
