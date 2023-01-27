#include "clustering.h"

class DBSCAN : public ClusteringAlgorithm {
 public:
  DBSCAN(double eps, int min_samples)
      : m_eps(eps), m_min_samples(min_samples){};
  void fit(const std::vector<Hit>& hits);
  std::vector<NeutronEvent> get_events(const std::vector<Hit>& hits);
  ~DBSCAN() = default;

 private:
  double m_eps;          // The maximum distance between two points
  double m_min_samples;  // The minimum number of points in a cluster
};
