#include "clustering.h"
#include "peakfitting.h"

struct Cluster {
  int x_min, y_min, x_max, y_max;
  int spidertime;
  int label, size;
};

/**
 * @brief Class for the AdaptiveBoxSearch (ABS) algorithm
 *
 */
class ABS : public ClusteringAlgorithm {
 public:
  ABS(double r) : m_feature(r){};
  void fit(const std::vector<Hit>& data);
  void set_method(std::string method) { m_method = method; };
  void reset() { clusterLabels_.clear(); };
  std::vector<NeutronEvent> get_events(const std::vector<Hit>& data);
  ~ABS() = default;

 private:
  double m_feature;                  // feather range
  std::string m_method{"centroid"};  // method for centroid
  std::vector<int> clusterLabels_;   // The cluster labels for each hit
  const int numClusters_ = 128;      // The number of clusters use in runtime
  const int maxClusterSize_ = 10;    // The maximum cluster size
  const int spiderTimeRange_ = 3;    // The spider time range
  PeakFittingAlgorithm* peakFittingAlgorithm_;  // The clustering algorithm
};
