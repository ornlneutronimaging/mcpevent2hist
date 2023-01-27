#include "clustering.h"
#include "peakfitting.h"

struct Cluster {
  int x_min, y_min, x_max, y_max;
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
  std::vector<NeutronEvent> get_events(const std::vector<Hit>& data);
  ~ABS() = default;

 private:
  double m_feature;                  // feather range
  std::string m_method{"centroid"};  // method for centroid
  std::vector<int> clusterLabels_;   // The cluster labels for each hit
  ClusteringAlgorithm* peakFittingAlgorithm_;  // The clustering algorithm
};
