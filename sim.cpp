#include <iomanip>
#include <map>
#include <list>
#include <vector>
#include <iostream>
#include <cmath>

/////////////////
// Almost everything about the design and style of this program is *bad*.  
//   You should not build on it.  It is cobbled together from older stuff.  
//   Don't learn from it...
//
// You are going to use this to run different simulations for an in-class
//   exercise.  To do that, you may need to change parameters and then
//   re-compile. You can find the description of the parameters in 
//   the structure "SimParamsType".  The values of the parameters are set
//   in the function setParams() at the end of this file -- that is where
//   you can change their values.  Feel free to modify that routine as you
//   want, including reading in more parameters.  I don't recommend changing
//   anything else.
/////////////////

#define LBZERO 1

////////////////// 
// Section: Declare new types
////////////////// 
// This is the structure of parameters used in the simulation
typedef struct __SimParamsType {
  // Mean of the distribution that generates # of work units
  double meanX; 
  // The number of work units processed by a server per second
  double valR;
  // Mean of the distribution that generates the inter-arrival time
  //   Units: seconds
  double meanLambda;
  // Type of the Work Unit distribution
  // 1: Exponential
  // 0: Constant
  unsigned int workUnitDistribType;
  // Number of servers in the systems
  unsigned int numServers;
  // Time for the simulation to settle to a steady state
  //  Units: Seconds
  double initTime;
  // Time for the simulation to finish process (mostly likely) requests from the steadySimTime
  //  Units: Seconds
  double postTime;
  // Time for the steady-state simulation when we collect the stats for requests received during
  //  Units: Seconds
  //   this time
  double steadySimTime;
  // Simulation time step
  //  Units: Seconds
  double timeStep;
  // Time for the load balancer to process a request
  double lbTime;
} SimParamsType;

typedef struct __RequestType {
  double startTime;
  double endTime;
  double lbStartTime;
  double lbEndTime;
  double serverStartTime;
  double workUnits;
  double responseTime;
} RequestType;

typedef std::vector<RequestType> RequestVectorType;
typedef std::list<RequestType> RequestListType;

typedef struct __ServerType {
  double doneTime;
  bool busy;
  std::list<int> inQueue;
  std::list<int> curQueue;
} ServerType;

typedef std::vector<ServerType> ServerVectorType;

typedef struct __LoadBalancerType {
  double doneTime;
  bool busy;
  std::list<int> inQueue;
  std::list<int> curQueue;
} LoadBalancerType;
////////////////// 
// Section: End
////////////////// 

int setParams(SimParamsType &params,int argc, char *argv[]);
double requestArrivalTime(const SimParamsType &params);
double workUnits(const SimParamsType &params);
bool compareRT(const RequestType &first,const RequestType &second);
double avgResponseTime(const double initTime,const double simTime,const RequestVectorType &requests);
double PowFactRecur(unsigned n, const double numerator);
double ComputePQ(unsigned n, const double rho);
void simServerTimeStep(const double curT,const double timeStep,const double R,ServerType &s,RequestVectorType &requests);
void simServersTimeStep(const double curT,const double timeStep,const double R,ServerVectorType &servers,RequestVectorType &requests);
void assignToServer(const int index,ServerVectorType &servers);
void simLBTimeStep(const double curT,const double timeStep,const double lbTime,LoadBalancerType &lb,RequestVectorType &requests,ServerVectorType &servers);

int main(int argc, char *argv[])
{
  SimParamsType params;
  double totalTime;

  // Call a function to set the parameters for the simulation
  if (setParams(params,argc,argv) < 0) {
    exit(-1);
  }
  totalTime = params.initTime + params.steadySimTime + params.postTime;

  // Print out the parameters to the simulation
  std::cout<<"****************"<<std::endl<<"Number of servers: "<<params.numServers<<std::endl;
  std::cout<<"Lambda = "<<params.meanLambda<<", R = "<<params.valR<<std::endl;
  std::cout<<"Time for load balancer to handle one request: "<<params.lbTime<<std::endl;
  std::cout<<"Simulation Time Step: "<<params.timeStep<<std::endl;
  std::cout<<"SteadyStateSimTime: "<<params.steadySimTime<<", Init time: "<<params.initTime<<", EndTime: "<<params.postTime<<std::endl;
  if (params.workUnitDistribType == 0) {
    std::cout<<"The number of work units required for each request is a constant: "<<params.meanX<<std::endl;
  } else if (params.workUnitDistribType == 1) {
    std::cout<<"The number of work units required for each request is drawn from an exponential distribution with mean: "<<params.meanX<<std::endl;
  }

  // Print out values derived from the parameters
  double mu = params.valR/params.meanX;
  double rho = params.meanLambda/(mu*params.numServers);
  std::cout<<"****************"<<std::endl<<"mu = "<<mu<<std::endl;
  std::cout<<"Rho (utilization) = "<<rho<<std::endl;
  if (rho >= 1.0) {
    std::cout<<"WARNING: The expected utilization is >= 1.0, unlikely to have a steady state"<<std::endl;
  }
  std::cout<<"Expected time for an unloaded server to compute a single response is "<<params.meanX/params.valR<<std::endl;
  if ((params.workUnitDistribType == 1) && (params.numServers == 1) && (params.lbTime == 0.0)) {
    double Tr = (1.0/mu)/(1.0-rho);
    double Tr95 = Tr * log(100.0/(100.0-95.0));
    std::cout<<"Expected M/M/1: Avg= "<<Tr<<" 95th = "<<Tr95<<std::endl;
  } else if ((params.workUnitDistribType == 0) && (params.numServers == 1) && (params.lbTime == 0.0)) {
    double Tr = (1.0/mu)+(rho/(2.0*mu*(1.0-rho)));
    double Tr95 = Tr + 2.0*((1.0/(mu*(1.0-rho)))*sqrt((rho/3.0)-(rho*rho/12.0)));
    std::cout<<"Expected M/D/1: Avg= "<<Tr<<" 95th = "<<Tr95<<std::endl;
  } else if ((params.workUnitDistribType == 1) && (params.numServers > 1) && (params.lbTime == 0.0)) {
    double Pq = ComputePQ(params.numServers,rho);
std::cout<<"Pq "<<Pq<<std::endl;
    double Tr = (1.0/mu) + ((Pq)/((params.numServers*mu)-params.meanLambda));
    std::cout<<"Expected M/M/c: Avg= "<<Tr<<std::endl;
  }

  // Generate all of the requests ahead of time (don't have to do it this way)
  RequestVectorType requests;
  int numRequests;
  double nextRequestTime = requestArrivalTime(params);
  while (nextRequestTime < totalTime) {
    RequestType curRequest;
    curRequest.startTime = nextRequestTime;
    curRequest.endTime = -1.0;
    curRequest.lbStartTime = -1.0;
    curRequest.lbEndTime = -1.0;
    curRequest.workUnits = workUnits(params);
    curRequest.responseTime = -1.0;
    requests.push_back(curRequest);
    numRequests++;
    //std::cout<<"request at "<<nextRequestTime<<" with units = "<<curRequest.workUnits<<std::endl;
    nextRequestTime += requestArrivalTime(params);
  }

  // Print out statistics on the requests
  std::cout<<"****************"<<std::endl;
  std::cout<<"For entire simulated time: Actual number of requests = "<<numRequests<<", actual Lambda: "<<numRequests/totalTime<<std::endl;

  // Simulate the operation of the load balancer and servers
  double curT = 0.0;
  int lbQIndex = 0;
  ServerVectorType servers(params.numServers);
  LoadBalancerType loadBalancer;
  unsigned int numSteps = 0;
  while (curT < totalTime) {
    //std::cout<<"Time step: "<<curT<<std::endl;

    // simulate load_balancer

    // Add requests that arrive in the current time step to the lb's in queue
    while ((lbQIndex < numRequests) && (requests[lbQIndex].startTime <= curT)) {
      loadBalancer.inQueue.push_back(lbQIndex);
      lbQIndex++;
    }
    simLBTimeStep(curT,params.timeStep,params.lbTime,loadBalancer,requests,servers);

    // Sim the servers
    simServersTimeStep(curT,params.timeStep,params.valR,servers,requests);

    // increment to next time step;
    curT += params.timeStep;
    numSteps++;
  }
  std::cout<<"****************"<<std::endl;
  std::cout<<"Num Simulation Steps: "<<numSteps<<std::endl;

  avgResponseTime(params.initTime,params.steadySimTime,requests);

  return 0;
}

////////////////// 
// Section: Implementations of the routines to generate the 
//   number of work units and the inter-arrival time.
//
// Using C++ standard library random number routines
////////////////// 

#include <random>
// This is just to seed the random number generator.
//   We hope that it generates a new random seed every run. If it
//   doesn't, then multiple runs with the same parameters will yield
//   the same results.
std::random_device randDevice; 
// Select a random number generating engine for use in our distributions.
std::mt19937_64 randGenEngine (randDevice ());

double requestArrivalTime(const SimParamsType &params)
{
  static std::exponential_distribution<> rngRequest (params.meanLambda);
  return rngRequest(randGenEngine);
}

double workUnits(const SimParamsType &params)
{
  if (params.workUnitDistribType == 1) {
    static std::exponential_distribution<> rngRequest(1.0/params.meanX);
    return rngRequest(randGenEngine);
  } else if (params.workUnitDistribType == 0) {
    return (params.meanX);
  } else {
    exit(-1);
  }
}
////////////////// 
// Section: End
////////////////// 

////////////////// 
// Section: Compute statistics after the simulation run
////////////////// 
double ComputePQ(unsigned n, const double rho)
{
/*
    double p0;
    for (int k=0;k<=(n-1);k++) {
      p0 += PowFactRecur(k,rho*k);
    }
    std::cout<<"v1: "<<p0<<std::endl;
    p0 += PowFactRecur(n,rho*n)*(1.0/(1.0-rho));
    std::cout<<"v2: "<<p0<<std::endl;
    p0 = 1.0/p0;
    std::cout<<"v3: "<<p0<<std::endl;
    std::cout<<"v4: "<<PowFactRecur(n,rho*n)<<std::endl;
    double Pq = PowFactRecur(n,rho*n) * (p0/(1.0-rho));
*/
    
    double p0;
    for (int k=0;k<=(n-1);k++) {
      p0 += PowFactRecur(k,rho*k);
    }
    double Pq = 1.0 + (((1.0-rho)/PowFactRecur(n,rho*n)) * p0);
    Pq = 1.0/Pq;
    return Pq;
}

double PowFactRecur(unsigned n, const double numerator)
{
  if (n == 0) {
    return (1.0);
  } else {
    double val = numerator;
    val /= n;
    return(val * PowFactRecur(n-1,numerator));
  }
}

bool compareRT(const RequestType &first,const RequestType &second)
{
  if (first.responseTime < second.responseTime) { 
    return(true);
  } else {
    return(false);
  }
}

double avgResponseTime(const double initTime,const double simTime,const RequestVectorType &requests)
{
  double sumTime = 0.0;
  double sumWorkUnits = 0.0;
  int numRequests = 0;
  RequestListType goodRequests;
  int unfinishedRequests = 0;

  for (int i=0;i<requests.size();i++) {
    if (requests[i].startTime <= initTime) {
    } else if (requests[i].startTime >= (initTime+simTime)) {
    } else if (requests[i].responseTime > 0.0) {
      numRequests++;
      goodRequests.push_back(requests[i]);
    } else {
      unfinishedRequests++;
    }
  }
  if (numRequests > 0) {
    goodRequests.sort(compareRT);
    int percentileIndex = ceil(0.95*numRequests)-1;
    RequestListType :: iterator it;
    int cnt = 0;
    double time95;
    double sumLBTime = 0;
    double sumServerTime = 0;
    double sumLBWaitTime = 0;
    double sumFromLBTime = 0;
    double maxResponseTime = 0.0;
    for (it = goodRequests.begin();it != goodRequests.end();++it) {
      if (cnt == percentileIndex) {
        time95 = (*it).responseTime;
      }
      sumTime += (*it).responseTime;
      sumWorkUnits += (*it).workUnits;
      sumLBTime += (((*it).lbEndTime) - ((*it).lbStartTime));
      sumServerTime += (((*it).endTime) - ((*it).serverStartTime));
      sumLBWaitTime += (((*it).lbStartTime) - ((*it).startTime));
      sumFromLBTime += (((*it).serverStartTime) - ((*it).lbEndTime));
      if (maxResponseTime < (*it).responseTime) {
        maxResponseTime = (*it).responseTime;
      }
      cnt++;
    }
    std::cout<<"******************************************"<<std::endl;
    std::cout<<"Results from the steady state simulation period"<<std::endl;
    std::cout<<"Requests that didn't finish (not included in stats): "<<unfinishedRequests<<std::endl;
    std::cout<<"Num requests that completed: "<<numRequests<<std::endl;
    std::cout<<"Avg response time "<<sumTime/numRequests<<std::endl;
    std::cout<<"Max Response Time "<<maxResponseTime<<std::endl;
    std::cout<<"95th percentile Response Time: "<<time95<<", ratio (95th to avg) = "<<time95/(sumTime/numRequests)<<std::endl;
    std::cout<<"Avg time to wait for LB = "<<sumLBWaitTime/numRequests<<std::endl;
    std::cout<<"Avg time to LB request = "<<sumLBTime/numRequests<<std::endl;
    std::cout<<"Avg Server Compute Time = "<<sumServerTime/numRequests<<std::endl;
    std::cout<<"Avg time from LB to server starting compute = "<<sumFromLBTime/numRequests<<std::endl;
    std::cout<<"Avg Work Units per request: "<<sumWorkUnits/numRequests<<std::endl;
    return(sumTime/numRequests);
  } else {
    std::cout<<"Num requests "<<numRequests<<std::endl;
    return(0.0);
  }
}
////////////////// 
// Section: End
////////////////// 

////////////////// 
// Section: Simulate servers
////////////////// 
void simServerTimeStep(const double curT,const double timeStep,const double R,ServerType &s,RequestVectorType &requests)
{
    double timeLeft;
    if (s.busy) {
      timeLeft = curT - s.doneTime;
    } else {
      timeLeft = timeStep;
    }
    while ((s.curQueue.size() > 0) && (timeLeft >= 0.0)) {
      if (s.busy) {
        s.busy = false;
        int index = s.curQueue.front();
        requests[index].endTime = curT - timeLeft;
        requests[index].responseTime = requests[index].endTime - requests[index].startTime;
        s.curQueue.pop_front();
      }
      if (s.curQueue.size() > 0) {
        s.busy = true;
        int index = s.curQueue.front();
        requests[index].serverStartTime = curT-timeLeft;
        s.doneTime = requests[index].serverStartTime + (requests[index].workUnits / R);
        timeLeft -= (requests[index].workUnits / R);
      }
    }

    if (s.inQueue.size() > 0) {
      s.curQueue.splice(s.curQueue.end(),s.inQueue);
    }
}

void simServersTimeStep(const double curT,const double timeStep,const double R,ServerVectorType &servers,RequestVectorType &requests)
{
  for (int i=0;i<servers.size();i++) {
    simServerTimeStep(curT, timeStep, R, servers[i],requests);
  }
}

double serverLoad(const ServerType &server) 
{
  return(server.inQueue.size() + server.curQueue.size());
}
////////////////// 
// Section: End
////////////////// 

////////////////// 
// Section: Simulate Load Balancer
////////////////// 
void assignToServer(const int index,ServerVectorType &servers)
{
  int minIndex = 0;
  double minVal = serverLoad(servers[0]);
  for (int i=0;i<servers.size();i++) {
    double curVal = serverLoad(servers[i]);
    if (curVal == 0.0) {
      servers[i].inQueue.push_back(index);
      return;
    } else if (curVal < minVal) {
      minVal = curVal;
      minIndex = i;
    }
  }
  servers[minIndex].inQueue.push_back(index);
}

void simLBTimeStep(const double curT,const double timeStep,const double lbTime,LoadBalancerType &lb,RequestVectorType &requests,ServerVectorType &servers)
{
    double timeLeft;
    if (lb.busy) {
      timeLeft = curT - lb.doneTime;
    } else {
      timeLeft = timeStep;
    }
    while ((lb.curQueue.size() > 0) && (timeLeft >= 0.0)) {
      if (lb.busy) {
        lb.busy = false;
        int index = lb.curQueue.front();
        assignToServer(index,servers);
        requests[index].lbEndTime = curT - timeLeft;
        lb.curQueue.pop_front();
      }
      if (lb.curQueue.size() > 0) {
        lb.busy = true;
        int index = lb.curQueue.front();
        requests[index].lbStartTime = curT-timeLeft;
        lb.doneTime = requests[index].lbStartTime + lbTime;
        timeLeft -= lbTime;
      }
    }

    if (lb.inQueue.size() > 0) {
      lb.curQueue.splice(lb.curQueue.end(),lb.inQueue);
    }
}
////////////////// 
// Section: End
////////////////// 

//////////////////
// Section: Set the parameters
////////////////// 
int setParams(SimParamsType &params,int argc, char *argv[]) 
{
  if (argc < 4) {
    std::cout<<"Error in args: numServers lambda R"<<std::endl;
    return(-1);
  }
  sscanf(argv[1],"%d",&params.numServers);
  sscanf(argv[2],"%lf",&params.meanLambda);
  sscanf(argv[3],"%lf",&params.valR);

  params.meanX = 2.0; 
  params.workUnitDistribType = 1;

  params.initTime = 1.0;
  params.postTime = 1.0;
  params.steadySimTime = 5.0;
  params.timeStep = 1.0;
  params.timeStep /= (2048);

  params.lbTime = 0.01;

  return(0);
}
////////////////// 
// Section: End
////////////////// 

