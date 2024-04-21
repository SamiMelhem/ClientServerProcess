// your PA3 client code here
#include <fstream>
#include <iostream>
#include <thread>
#include <sys/time.h>
#include <sys/wait.h>

#include "BoundedBuffer.h"
#include "common.h"
#include "Histogram.h"
#include "HistogramCollection.h"
#include "TCPRequestChannel.h"

#include <vector>
#include <cstring>

// ecgno to use for datamsgs
#define EGCNO 1

using namespace std;

struct DataResponse{
    int patient_num;
    double data_value;
};

void patient_thread_function (int patient_num, int n, BoundedBuffer* request_buffer) {
    // functionality of the patient threads
    
    // take a patient num
    // for n requests, produce a datamsg(num, time, ECGNO) and push to request buffer
    //      - time dependent on current threads
    //      - at 0 -> time = 0.00, at 1 -> time = 0.004, at 2 -> time = 0.008, ...

    datamsg dataMessage(patient_num, 0.0, EGCNO);
    for (int i = 0; i < n; ++i){
        vector<char> request(sizeof(datamsg));
        memcpy(request.data(), &dataMessage, sizeof(datamsg));

        request_buffer->push(request.data(),sizeof(datamsg));
        dataMessage.seconds += 0.004;
    }
}

void file_thread_function (string filename, BoundedBuffer* request_buffer, int m, TCPRequestChannel* chan) {
    // functionality of the file thread

    // Get the file size (filemsg, 0, 0, appended with the final name)
    // open output file; allocate the memory using fseek(); close the file
    // while offset < f_size, produce a filemsg(offset, m)+filename and push to request buffer:
    //      - incrementing offset; and be careful with the final message

    // cout << "Starting file transfer for: " << filename << endl;

    int len = sizeof(filemsg) + (filename.size() + 1);
    char* buf = new char[len];
    filemsg fm(0,0);
    memcpy(buf, &fm, sizeof(filemsg));
    strcpy(buf + sizeof(filemsg), filename.c_str());
    chan->cwrite(buf, len);

    // Recieve the response
    int64_t file_size;
    chan->cread(&file_size, sizeof(int64_t));

    // Open output file and allocate memory using fseek
    string outputFilePath = "received/" + filename;
    FILE* outfile = fopen(outputFilePath.c_str(), "wb"); 
    fseek(outfile, file_size - 1, SEEK_SET); 
    fputc('\0', outfile); 
    fclose(outfile); // Close the file

    // cout << "Finished file pre-allocation for: " << filename << endl;

    // Send chunk requests for the entire file
    __int64_t offset = 0; 
    // cout << "Sending file chunk requests for: " << filename << endl;
    while (offset < file_size) {
        int length = static_cast<int>(min(static_cast<__int64_t>(m), file_size - offset)); 
        filemsg fm(offset, length);

        vector<char> request(sizeof(filemsg) + filename.length() + 1); // Request buffer
        memcpy(request.data(), &fm, sizeof(filemsg)); 
        strcpy(request.data() + sizeof(filemsg), filename.c_str()); 
        request_buffer->push(request.data(), request.size()); 

        offset += length; 
    }

    delete[] buf;
}

void worker_thread_function (BoundedBuffer* request_buffer, BoundedBuffer* response_buffer, TCPRequestChannel* chan) {
    // functionality of the worker threads

    // forever loop
        // pop message from request_buffer
        // view line 120 in server (process_request function) for how to decide current message
        // send the message across a FIFO channel
        // collect response
        // if DATA:
        //      - create pair of p_no from message and response from server
        //      - push that pair to the response_buffer
        // if FILE:
        //      - collect the filename from the message you pop from the buffer
        //      - open the file in update mode
        //      - fseek(SEET_SET) to offset of the filemsg
        //      - write the buffer from the server

    while (true) {
        char* message = new char[MAX_MESSAGE]; // Buffer to hold request data
        int messageSize = request_buffer->pop(message, MAX_MESSAGE); // Pop message from request buffer

        MESSAGE_TYPE *m = (MESSAGE_TYPE*) message;
        if (*m == QUIT_MSG){
            delete[] message;
            break;
        }

        chan->cwrite(message, messageSize);

        if (*m == DATA_MSG) {
            int patient_id = ((datamsg*) message)-> person;

            double data_value;
            int responseSize = sizeof(double);
            chan->cread(&data_value, responseSize);

            pair<int, double> data_pair(patient_id, data_value);
            
            response_buffer->push((char*)&data_pair, sizeof(data_pair));
        } 
        else if (*m == FILE_MSG){ // Handling FILE request
            // collect the filename and filedata
            filemsg* fm = (filemsg*) message;
            string filename = (char*)(fm+1); // Extract filename from the message
            char* filedata = new char[MAX_MESSAGE]; //  Write the buffer from the server

            size_t nbytes = chan->cread(filedata, MAX_MESSAGE);  // Read from the channel
            // if (nbytes < 0) {
            //     perror("Failed to read from socket");
            //     delete[] filedata;
            //     delete[] message;
            //     continue;  // Skip this iteration on read error
            // }

            // chan->cread(filedata, MAX_MESSAGE);
            // open the file in update mode
            string filePath = "received/"+filename;
            FILE* outfile = fopen(filePath.c_str(), "r+"); // makes the file open for reading and writing
            if (outfile == nullptr) {
                perror("Failed to open file");
                delete[] filedata;
                delete[] message;
                continue;  // Skip this iteration on file open error
            }

            // // If the file doesn't exist, then create it
            // if (outfile == nullptr)
            //     outfile = fopen(filePath.c_str(), "wb+"); 
            // Add to the created/existed file via fseek(SEET_SET) to offset the filemsg
            fseek(outfile, fm->offset, SEEK_SET);
            if (fwrite(filedata, 1, nbytes, outfile) != nbytes) {
                perror("Failed to write all bytes to file");
            }
            fclose(outfile); // Close the file

            delete[] filedata;
        }
        delete[] message;
    }
}

void histogram_thread_function (BoundedBuffer* response_buffer, HistogramCollection* hc) {
    // functionality of the histogram threads

    // forever loop
    // pop repsonse from the response buffer
    // call HC::update(resp->p_no, resp->double)
    // Send the thread to quit


    while (true){
        char* response = new char[MAX_MESSAGE]; // Buffer to hold response data
        response_buffer->pop(response, MAX_MESSAGE); // Pop response into buffer
        
        // Extracting the patient num and value from the response buffer
        pair<int, double>* data_pair = (pair<int, double>*) response;

        int patient_id = data_pair->first;
        double data_value = data_pair->second;
        
        // Check for a special condition to break the loop if necessary
        // For example, a specific patient_id might signal that all data has been processed
        if (data_value == -1 and patient_id == 0) { 
            delete[] response;
            break; 
        }
        
        // Update the histogram for the given patient
        hc->update(patient_id, data_value);
        delete[] response;
    }
}


int main (int argc, char* argv[]) 
{
    int n = 1000;	// default number of requests per "patient"
    int p = 10;		// number of patients [1,15]
    int w = 100;	// default number of worker threads
	int h = 20;		// default number of histogram threads
    int b = 20;		// default capacity of the request buffer
	int m = MAX_MESSAGE;	// default capacity of the message buffer
	string f = "";	// name of file to be transferred
    string host = "127.0.0.1"; // default localhost
    string port = "1234"; // default port number
    
    // read arguments
    int opt;
	while ((opt = getopt(argc, argv, "n:p:w:h:b:m:f:a:r:")) != -1) {
		switch (opt) {
			case 'n':
				n = atoi(optarg);
                break;
			case 'p':
				p = atoi(optarg);
                break;
			case 'w':
				w = atoi(optarg);
                break;
			case 'h':
				h = atoi(optarg);
				break;
			case 'b':
				b = atoi(optarg);
                break;
			case 'm':
				m = atoi(optarg);
                break;
			case 'f':
				f = optarg;
                break;
            case 'a':  // IP address
                host = optarg;
                break;
            case 'r':  // Port
                port = optarg;
                break;
		}
	}
    
	// // fork and exec the server
    // int pid = fork();
    // if (pid == 0) {
    //     execl("./server", "./server", "-m", (char*) to_string(m).c_str(), nullptr);
    // }
    
	// initialize overhead (including the control channel)
	TCPRequestChannel* chan = new TCPRequestChannel(host, port);
    BoundedBuffer request_buffer(b);
    BoundedBuffer response_buffer(b);
	HistogramCollection hc;
    // vector of TCPs (w elements)
    vector<TCPRequestChannel*> TCPs;

    // vector of producer threads (if data, p elements; if file, 1 element)
    vector<thread> producer_threads;
    // vector of worker threads (w elements)
    vector<thread> worker_threads;
    // vector of histogram threads (if data, h, elements; if file, zero elements)
    vector<thread> histogram_threads;

    // making histograms and adding to collection
    for (int i = 0; i < p; i++) {
        Histogram* h = new Histogram(10, -2.0, 2.0);
        hc.add(h);
    }

    // // making worker channels
    // for (int i = 0; i < w; i++){
    //     TCPRequestChannel* new_chan = new TCPRequestChannel(host, port);
    //     TCPs.push_back(new_chan);
    // }
	
	// record start time
    struct timeval start, end;
    gettimeofday(&start, 0);

    /* create all threads here */
    // bool data;
    // if data:
    //      - create p patient_threads (store them in producer vector)
    // if file:
    //      - create 1 file_thread (store in producer vector)
    //
    // create w worker threads
    //      - create w worker_threads (store them in worker vector)
    //          -> create w channels (store in the FIFO vector)
    //
    // if data:
    //      - create h histogram_threads (store them in hist vector)

    TCPRequestChannel* file_channel;
    bool dataRequest = f.empty(); // If 'f' is empty, then it's a data request; otherwise, it's a file request 
    // Create a single file producer thread if 'f' is not empty
    // Create patient producer threads if 'f' is empty
    if (dataRequest) {
        for (int i = 0; i < p; ++i) { producer_threads.emplace_back(patient_thread_function, i+1, n, &request_buffer); }
    } else {
        // Create the file channel via NEWCHANNEL_MSG and channels
        char channels[100];
        MESSAGE_TYPE fchan = NEWCHANNEL_MSG;
        chan->cwrite(&fchan, sizeof(fchan));
        chan->cread(channels, sizeof(channels));

        file_channel = new TCPRequestChannel(host, port); 
        producer_threads.emplace_back(file_thread_function, f, &request_buffer, m, file_channel);
    }


    // Create worker threads and their channels
    for (int i = 0; i < w; i++) 
    {
        char channels2[100];
        MESSAGE_TYPE wchan = NEWCHANNEL_MSG;
        chan->cwrite(&wchan, sizeof(wchan));
        chan->cread(channels2, sizeof(channels2));

        // store in the FIFO vector
        TCPs.push_back(new TCPRequestChannel(host, port));

        worker_threads.emplace_back(worker_thread_function, &request_buffer, &response_buffer, TCPs.at(i));
    }

    if (dataRequest) {
        for (int i = 0; i < h; i++) {
            // Histogram Thread
            histogram_threads.emplace_back(histogram_thread_function, &response_buffer, &hc);
        }
    }

	/* join all threads here */
    // iterate over all thread vectors, calling .join
    //      - order is important; producers before consumers

    // Join all producer threads
    for (auto& thread : producer_threads) { thread.join(); }


    // Push QUIT_MSG for each worker thread

    // Quit for worker threads
    MESSAGE_TYPE quitMessage = QUIT_MSG;
    for (int i = 0; i < w; i++) {
        // Transfer the quitMessage to a vector 
        vector<char> quitMessageData(sizeof(quitMessage));
        memcpy(quitMessageData.data(), &quitMessage, sizeof(quitMessage));

        // Push the quitMessage via a vector to RequestBuffer
        request_buffer.push(quitMessageData.data(), sizeof(quitMessage));
    }

    // Worker Threads
    for (auto& thread : worker_threads) { thread.join(); }

    // Quit for histograms
    pair<int, double> he(0, -1.0); // The quit message for Histogram threads
    for (int i = 0; i < h; i++) 
    {
        // Transfer the Histogram quit message to a vector
        vector<char> histogramExitChar(sizeof(pair<int, double>));
        memcpy(histogramExitChar.data(), &he, sizeof(pair<int, double>));
        // Push the pair into the response buffer
        response_buffer.push(histogramExitChar.data(), sizeof(pair<int, double>));
    }

    // Histogram Threads
    for (auto& thread: histogram_threads) { thread.join(); }

	// record end time
    gettimeofday(&end, 0);

    // print the results
	if (f == "") {
		hc.print();
	}
    int secs = ((1e6*end.tv_sec - 1e6*start.tv_sec) + (end.tv_usec - start.tv_usec)) / ((int) 1e6);
    int usecs = (int) ((1e6*end.tv_sec - 1e6*start.tv_sec) + (end.tv_usec - start.tv_usec)) % ((int) 1e6);
    cout << "Took " << secs << " seconds and " << usecs << " micro seconds" << endl;

    // quit and close all channels in FIFO vector
    for (auto channel : TCPs) {
        channel->cwrite((char*)quitMessage, sizeof(quitMessage));
        delete channel;
    }

    if (!dataRequest) {
        file_channel->cwrite((char*)quitMessage, sizeof(quitMessage));
        delete file_channel;
    }

	// quit and close control channel
    MESSAGE_TYPE q = QUIT_MSG;
    chan->cwrite ((char *) &q, sizeof (MESSAGE_TYPE));
    cout << "All Done!" << endl;
    delete chan;

	// // wait for server to exit
	// wait(nullptr);
}