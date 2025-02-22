#include "include/concurrent_hashmap.h"
#include "include/IPCManager.hpp"
#include "include/ipc_channel.h"
#include "include/logger.h"
#include "include/message.h"
#include "include/message_queue.h"
#include "include/UIUpdater.hpp"
#include "include/vectorizer.h"
#include "include/WorkerPool.hpp"

#ifdef _WIN32 // this is for testing purposes right now
// access from ipc_channel.cpp and vectorizer.cpp in src, will connect a different way later
// ipc connection (faking it for now)
extern IPCChannel* createSyntheticIPCChannel(const std::string& filepath);
#else   /// real pipe on linux
extern IPCChannel* createRealIPCChannel(const std::string& pipePath);
#endif
// vectorization module (also faking vector embeddings for now)
extern Vectorizer* createVectorizer();


int main() {
	/*  PSEUDOCODE
	 *	initialize log (timestamp, content)
	 *	initialize thread safe messagequeue
	 *	initiazlize conccurent hashmap where group_id -> bucket (vector objects)
	 *	bucket_locks for concurrent hashmap ->
	 *  define/set threshold
	 *
	 *  establish ipc channel
	 *
	 *	start ipc reciever thread -> need ipc channel and messagequeue access
	 *		log start of the thread
	 *		read from ipc channel, validate message content, parse and add to message queue
	 *		log any errors
	 *		log end of thread
	 *		Note any errors, retry or skip
	 *
	 *	start worker thread pool -> num of worker threads, messagequeue access, concurrent hashmap access
	 *		log start of start worker pool and start of each individual thread
	 *		for each vectorization worker
	 *			message = dequeue messagefromqueue
	 *			get embedding from message
	 *
	 *			create objectwithembedding {embedding, groupid, }
	 *
	 *			lock to bucket in concurrent hashmap
	 *			add embedding to bucket
	 *			unlock bucket
	 *		log end of worker threadpool
	 *
	 *	start thread for concurrent hashmap to UI -> concurrent hashmap access
	 *		wait x time, loop over bucket, update ui
	 *		define increment
	 *		while (true) wait increment each time
	 *			copy bucket from lock
	 *			similarity search of bucket
	 *			store result
	 *			log stored result
	 */

	Logger::log("Main::Start");

	// initialize shared memory/datastructures
	MessageQueue<Message> messageQueue;
	ConcurrentHashMap groupBuckets;

	// connect to fake IPC channel (just reading from data/synthetic_ipc.csv)
#ifdef _WIN32
	IPCChannel* ipcChannel = createSyntheticIPCChannel("data/synthetic_ipc.csv");
#else // real ipc
	IPCChannel* ipcChannel = createRealIPCChannel("/tmp/ipc_pipe");
#endif
	// connect to fake vector embeddings
	Vectorizer* vectorizer = createVectorizer();

	// initialize IPC manager
	IPCManager ipcManager(ipcChannel, messageQueue);
	ipcManager.start();

	// define num of workers (for project requirements of 10 threads there are 2 outside of this worker pool 1 for IPC manager, 1 for UI Updater,)
	constexpr size_t numWorkerThreads = 6;
	WorkerPool workerPool(messageQueue, groupBuckets, vectorizer, numWorkerThreads);
	workerPool.start();

	// initialize UI updater -> set time to update 5 seconds for now
	UIUpdater uiUpdater(groupBuckets, std::chrono::seconds(5));
	uiUpdater.start();

	// time for this to run
	std::this_thread::sleep_for(std::chrono::seconds(20));

	ipcManager.stop();
	uiUpdater.stop();
	workerPool.stop();

	delete ipcChannel;
	delete vectorizer;

	Logger::log("Main::Exit");
	return 0;
}
