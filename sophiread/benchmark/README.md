# Benchmarking Procedure for Misclassification

Currently the parameters for clustering algorithms (e.g ABS, DBSCAN) are hard-coded in the source code. The hard-coded numbers are initial guesses for the clustering parameters, which may or may not be the optimal values. 

The goal of the benchmarking procedure is to establish a routine and metrics that can help us find the optimal parameters for different clustering algorithms. 

There are two possible types of misclassification:
- 1 cluster is misclassified as $n$ clusters.
- $n$ clusters are misclassified as 1 cluster.

To tackle the first case (1 cluster is misclassified as multiple clusters):
1. Run `sophiread` with an initial guess of the clustering algorithm.
2. Generate `events.h5` that provides a list of neutron events with `x,y,toa_avg` information.
3. Find duplicated events based on spatial and temporal constraints.
4. Visualize misclassified clusters to confirm misclassification.
5. Report the percentage of duplicated events. 

To tackle the second case (multiple clusters are misclassified as a single cluster):
1. Identify large clusters that may be made up of multiple smaller clusters. 
   - gather statistics of the cluster results (mean/median/std of the cluster along spatial axis and temporal axis)
   - isolate clusters that are at the tail of the distribution (1 std, 2 std, etc)
2. Perform unsupervised clustering (e.g. DBSCAN) to find sub-clusters within large clusters.
3. Investigate the clusters from each group
   - those remaining a single cluster -> need to confirm if these are indeed caused by neutron or perhaps other phenomenon
   - those got sub-clustered -> check their percentage and adjust the clustering parameter, the ultimate goal is to minimize this kind of mis-classification.
4. Report the percentage of misclassification and correct classification. 

Once the routine has been established with relevant metrics, this procedure can be helpful in finding the optimal parameters for clustering parameters (parameter tuning).