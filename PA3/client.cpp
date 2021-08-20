#include "common.h"
#include "BoundedBuffer.h"
#include "Histogram.h"
#include "common.h"
#include "HistogramCollection.h"
#include "FIFOreqchannel.h"
using namespace std;


struct p_arg
{
    int n;
    int p;
    int w;
    int b;
    string file_name;
    
    BoundedBuffer * buff;
    p_arg(int n,int p, BoundedBuffer * buff):n(n),p(p),buff(buff){};
};
struct w_arg
{
    BoundedBuffer * buff;
    FIFORequestChannel* chan;
    HistogramCollection * hc;
    w_arg(BoundedBuffer * buff, FIFORequestChannel* chan,HistogramCollection * hc):buff(buff),chan(chan),hc(hc){};
};


void * patient_function(void* arg)
{
    /* What will the patient threads do? */
    p_arg * dat_in = (p_arg*) arg;
    for (double i = 0; i < dat_in->n*.004; i=i+0.004)
    {
        if (i>59.996)
        {
            break;
        }
        //cout << dat_in->p << " " << i << endl;
        datamsg * data = new datamsg(dat_in->p,i,1);
        // cout<< data->person<< " intial_person"<<endl;
        // cout<< data-> seconds<< " intial_seconds"<<endl;
        // cout<< data-> ecgno << " intial_ECG"<<endl;
        char * temp = (char*)data;
        vector<char> temp_vec (temp,temp+sizeof(datamsg));
        dat_in->buff->push(temp_vec);    
    }
    
    
}

void *worker_function(void* arg)
{
    w_arg * dat_out = (w_arg*) arg;
    while (true)
    {   

    
        
        //w_arg * dat_out = (w_arg*) arg;
        vector<char> temp_vec = dat_out->buff->pop();
        char * temp = reinterpret_cast<char*>(temp_vec.data());
        datamsg * data = (datamsg*) temp;
        //cout<< data->person<< " person"<<endl;
        //cout<< data-> seconds<< " seconds"<<endl;
        //cout<< data-> ecgno << " ECG"<<endl;
        if (data->mtype == QUIT_MSG)
        {
            //cout<<"ran "<<endl;
            MESSAGE_TYPE m = QUIT_MSG;
            dat_out->chan->cwrite ((char*)&m, sizeof(QUIT_MSG));
            delete dat_out->chan;
            //cout<<"broke here"<<endl;
            return nullptr;
        }
        dat_out->chan->cwrite((char*)data, sizeof(datamsg));
        char* msg = dat_out->chan->cread();
        dat_out->hc->update(*(double *) msg,data->person);
        //cout << "The ECG data for this patient: "<< *(double *) msg << endl;
    }
}


int main(int argc, char *argv[])
{
    int option;
    bool file_run = false;
    string file;
    int n = 15000;    //default number of requests per "patient"
    int p = 15;     // number of patients [1,15]
    int w = 500;    //default number of worker threads
    int b = 1; 	// default capacity of the request buffer, you should change this default
	int m = MAX_MESSAGE; 	// default capacity of the file buffer
    srand(time_t(NULL));
     while((option = getopt(argc, argv, "n:p:w:b:f"))!=-1)
  {
    switch (option)
    {
      case 'n':
      n = atoi(optarg);
      //cout << pat << endl;
      break;
  
      case 'p':
      p = atoi(optarg);
      break;

      case 'w':
      w = atoi(optarg);
      break;
  
      case 'b':
      b = atoi(optarg);
      //cout << file << endl;
      break;
      
      case 'f':
      file = optarg;
      file_run = true;
      break;

    }
  }
    
    
    int pid = fork();
    if (pid == 0){
		// modify this to pass along m
        execl ("dataserver", "dataserver", (char *)NULL);
        
    }
    
	FIFORequestChannel* chan = new FIFORequestChannel("control", FIFORequestChannel::CLIENT_SIDE);
    BoundedBuffer request_buffer(b);
	HistogramCollection hc;
	
	
	
    struct timeval start, end;
    gettimeofday (&start, 0);

    /* Start all threads here */
    
    // patient threads
    vector<thread> pat_threads;
    for (int i = 0; i < p; i++)
    {
        Histogram * temp = new Histogram(10,-2,2);
        hc.add(temp); 
        p_arg * pat = new p_arg(n,i+1,&request_buffer);
        pat_threads.push_back(thread(patient_function,pat));
    }
    
    // worker threads
    vector<thread> work_threads;
    for (int i = 0; i < w; i++)
    {
        
        new_channel_msg new_chan_msg;
        chan->cwrite((char*)&new_chan_msg, sizeof(new_chan_msg));
        char* new_chan_name = chan->cread();
        //cout << "Channel Name is: " << new_chan_name << endl;
        FIFORequestChannel* new_chan = new FIFORequestChannel (new_chan_name, FIFORequestChannel::CLIENT_SIDE);
        //work->chan = new_chan;
        w_arg * work = new w_arg(&request_buffer,new_chan,&hc);
        work_threads.push_back(thread(worker_function,work));
    }
    
    /* Join all threads here */

    //ending patient threads
    for (int i = 0; i < p; i++)
    {
        pat_threads[i].join();
    }
    //cout <<"pat_threads joined"<<endl;

    //char* quit_th = (char*)(new quit_msg);
    //vector<char>quit(quit_th, quit_th + sizeof(quit_msg));
    for (int i = 0; i < w; i++)
    {
        MESSAGE_TYPE q = QUIT_MSG;
        char* temp = (char*)&q;
        vector<char> quit(temp,temp+(sizeof(QUIT_MSG)));
        request_buffer.push(quit);
    }
    
    //ending worker threads
    for (int i = 0; i < w; i++)
    {
       work_threads[i].join();
    }
    
    gettimeofday (&end, 0);
	hc.print ();
    int secs = (end.tv_sec * 1e6 + end.tv_usec - start.tv_sec * 1e6 - start.tv_usec)/(int) 1e6;
    int usecs = (int)(end.tv_sec * 1e6 + end.tv_usec - start.tv_sec * 1e6 - start.tv_usec)%((int) 1e6);
    cout << "Took " << secs << " seconds and " << usecs << " micor seconds" << endl;

    MESSAGE_TYPE q = QUIT_MSG;
    chan->cwrite ((char *) &q, sizeof (MESSAGE_TYPE));
    cout << "All Done!!!" << endl;
    delete chan;
    
}
