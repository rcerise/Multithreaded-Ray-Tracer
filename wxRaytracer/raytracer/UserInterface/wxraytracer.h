#ifndef _WXRAYTRACER_H_
#define _WXRAYTRACER_H_

/**
 * Ray Tracer skeleton
 *
 * Author : Sverre Kvaale <sverre@kvaale.com>
 * Multithreading Author: Daniel Rosser <danoli3@gmail.com>
 * Version: 0.9.1
 */
#include <wx/wx.h>
#include <wx/frame.h>
#include <wx/thread.h>
#include <wx/menu.h>
#include <wx/app.h>
#include "World.h"
#include <vector>
#include "MultiThread.h"

#include <stdlib.h>
#include <assert.h>
#include <map>
#include <list>

#define SAMPLE_HACK 1; // 0 still has Environment Light sample problem
					   // 1 creates multiple instances of World (1 per thread D:)

using namespace std;

class wxraytracerFrame;
class RenderCanvas;
class RenderThread;
class WorkerThread;
class RenderPixel;

DECLARE_EVENT_TYPE(wxEVT_THREAD, -1)

class tJOB
{
  public:
    enum tCOMMANDS                    // list of commands that are currently implemented
    {
      eID_THREAD_EXIT=wxID_EXIT,	  // thread should exit or wants to exit
      eID_THREAD_NULL=wxID_HIGHEST+1, // dummy command
      eID_THREAD_STARTED,		      // worker thread has started OK
      eID_THREAD_JOB,				  // process normal job
      eID_THREAD_JOBERR				  // process errorneous job after which thread likes to exit
    }; // enum tCOMMANDS
  
    tJOB() : cmd(eID_THREAD_NULL), theJob() {}
	tJOB(tCOMMANDS cmd, const wxString& arg) : cmd(cmd), arg(arg), theJob() {}
	tJOB(tCOMMANDS cmd, const wxString& arg, PixelPoints jobID) : cmd(cmd), arg(arg)
	{ theJob.origin = jobID.origin; theJob.end = jobID.end; }
    tCOMMANDS cmd; wxString arg;
	PixelPoints theJob;
}; // class tJOB
  
  class Queue
  {
  public:
    enum tPRIORITY { eHIGHEST, eHIGHER, eNORMAL, eBELOW_NORMAL, eLOW, eIDLE }; // priority classes

    Queue(wxEvtHandler* parent) : parent(parent) {}

    void AddJob(const tJOB& job, const tPRIORITY& priority=eLOW);			   // push a job with given priority class onto the FIFO
    
    tJOB Pop();

    void Report(const tJOB::tCOMMANDS& cmd, const wxString& sArg=wxEmptyString, int iArg=0); // report back to parent  

    size_t Stacksize();
  
  private:
    wxEvtHandler* parent;
    std::multimap<tPRIORITY, tJOB> jobs; // multimap to reflect prioritization: values with lower keys come first, newer values with same key are appended
    wxMutex mutexQueue;					 // protects queue access
    wxSemaphore queueCount;				 // semaphore count reflects number of queued jobs
};


  
class WorkerThread : public wxThread
{
  public:
    WorkerThread(Queue* pQueue, RenderCanvas* canvas, World* world, int id=0) : wxThread(), queue(pQueue), canvas(canvas), world(world), id(id) { assert(pQueue); wxThread::Create(); }
  
  private:
    Queue* queue;
    int id;
	RenderCanvas* canvas;
	World* world;
  
    virtual wxThread::ExitCode Entry();
  
    virtual void OnJob();
    

}; // class WorkerThread : public wxThread



class wxraytracerapp : public wxApp
{
public:
   virtual bool OnInit();
   virtual int OnExit();
   virtual void SetStatusText(const wxString&  text, int number = 0);

private:
   wxraytracerFrame *frame;
   DECLARE_EVENT_TABLE()
};

class wxraytracerFrame : public wxFrame
{
public:
   wxraytracerFrame(const wxPoint& pos, const wxSize& size);
   
   //event handlers
   void OnQuit( wxCommandEvent& event );
   void OnClose( wxCommandEvent& event );
   void OnOpenFile( wxCommandEvent& event );
   void OnSaveFile( wxCommandEvent& event );
   void OnRenderStart( wxCommandEvent& event );
   void OnRenderCompleted( wxCommandEvent& event );
   void OnRenderPause( wxCommandEvent& event );
   void OnRenderResume( wxCommandEvent& event );

private:
   RenderCanvas *canvas; //where the rendering takes place
   wxString currentPath; //for file dialogues  
   DECLARE_EVENT_TABLE()
};

//IDs for menu items
enum
{
   Menu_File_Quit = 100,
   Menu_File_Open,
   Menu_File_Save,
   Menu_Render_Start,
   Menu_Render_Pause,
   Menu_Render_Resume
};

class RenderCanvas: public wxScrolledWindow
{
	enum { eQUIT=wxID_CLOSE, eSTART_THREAD=wxID_HIGHEST+100 };
public:
   RenderCanvas(wxWindow *parent);
   virtual ~RenderCanvas(void);
    
   void SetImage(wxImage& image);
   wxImage GetImage(void);
   
   virtual void OnDraw(wxDC& dc);
   void renderStart(void);
   void renderPause(void);
   void renderResume(void);
   void OnRenderCompleted( wxCommandEvent& event );
   void OnTimerUpdate( wxTimerEvent& event );
   void OnNewPixel( wxCommandEvent& event );

   void OnStart(wxCommandEvent& WXUNUSED(event)); // start one worker thread
   void OnStop(wxCommandEvent& WXUNUSED(event));   // stop one worker thread
   void OnJob(wxCommandEvent& event); // handler for launching a job for worker thread(s)
   void addJob();
   void OnThread(wxCommandEvent& event); // handler for thread notifications
   void addThread();
   void OnQuit();
   void OnStart();

   int totalThreads; 
   int threadNumber;
   int divisions;
   World* w;
   RenderThread* manager;
protected:
   wxBitmap *theImage;
   wxStopWatch* timer;
   
  
   Queue* queue;   
   std::list<int> threads; 
   vector<WorkerThread*> theThreads;
#if SAMPLE_HACK>0
   vector<World*> theWorlds;
#endif
   vector<WorkerThread*>::iterator iter;

private:    
   long pixelsRendered;
   long pixelsToRender;
   wxTimer updateTimer;
   
   DECLARE_EVENT_TABLE()
};



class RenderPixel
{
public:
   RenderPixel(int x, int y, int red, int green, int blue);

public:
   int x, y;
   int red, green, blue;
};


DECLARE_EVENT_TYPE(wxEVT_RENDER, -1)
#define ID_RENDER_COMPLETED 100
#define ID_RENDER_NEWPIXEL  101
#define ID_RENDER_UPDATE    102

class RenderThread : public wxThread
{
public:
   RenderThread(RenderCanvas* c) : wxThread(wxTHREAD_JOINABLE), canvas(c), timer(NULL) {}
   virtual void *Entry();
   virtual void OnExit();
   virtual void setPixel(int x, int y, int red, int green, int blue);
   virtual void setPixel(const list<RenderedInt>& rendered);
private:
   void NotifyCanvas();
   RenderCanvas* canvas;   
   vector<RenderPixel*> pixels;
   long lastUpdateTime; 
   wxStopWatch* timer;

   wxMutex mutex;
};


#endif
