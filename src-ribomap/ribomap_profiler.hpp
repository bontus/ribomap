#ifndef RIBOMAP_PROFILER_HPP
#define RIBOMAP_PROFILER_HPP

#include <unordered_map>
#include <vector>
#include <string>
#include <random>
#include <iostream>

#include <boost/dynamic_bitset.hpp>

using namespace std;

//------class forward declarations------//
class transcript_info;
class fasta_reader;

//------copy aliases from other headers to here------//
struct fp_record;
using fp_list_t = vector<fp_record>;

//------aliases in this header------//
using rid_t = unsigned;

//------const------//
const double EPSILON = 1e-7;
const unsigned SEED = 619048235;

//------classes------//
//------profile info per transcript------//
struct tprofile{
  // expected read count on each position 
  double tot_count;
  vector<double> count;
  // transcript abundance from sailfish
  double tot_abundance;
};
//------expected count info per read per transcript-location pair------//
struct read_loc_count {
  rid_t tid;
  rid_t loc;
  double count;
};
//------count info per read------//
struct read_count { 
  double tot_count;
  vector<read_loc_count> loc_count_list;
};

/***
 * class ribo_profile:
 * store the profile from the model, the expected read assignment, and perform the EM
 */
class ribo_profile{
public:
  vector<read_count> read_count_list; 
  unordered_map<rid_t, rid_t> refID2pID; //ref index from fasta --> profile index
  ribo_profile(const transcript_info& tinfo, const char* sf_fname, double abundance_cutoff = 1);
  bool initialize_read_count(const fp_list_t& fp_codon_list, bool normalize = true);
  vector<rid_t> get_expressed_transcript_ids() const;
  bool is_expressed(rid_t refID) const { return nonzero_abundance_vec[refID]; }
  const vector<double>& get_read_assignments(rid_t t) const { return profile[t].count;}
  double get_count(rid_t t, rid_t i) const { return profile[t].count[i]; }
  double get_tot_abundance(rid_t t) const { return profile[t].tot_abundance; }
  double get_tot_count(rid_t t) const { return profile[t].tot_count; }
  rid_t get_transcript_index(rid_t refID) const { return refID2pID.at(refID); }
  size_t len(rid_t t) const { return profile[t].count.size(); }
  size_t number_of_transcripts() const { return profile.size(); }
  bool single_map_read_count(const fp_list_t& fp_codon_list);
  bool assign_reads();
  void update_count_profile();
private:
  vector<tprofile> profile;
  // bit vector: 1--transcript has non-zero abundance; 0--zero abundance
  boost::dynamic_bitset<> nonzero_abundance_vec; 
  double total_read_count;
  void reset_read_count();
  void include_abundant_transcript(rid_t refID) { nonzero_abundance_vec.set(refID); }
  void add_read_count(rid_t t, rid_t i, double count) { profile.at(t).count.at(i) += count; }
  void add_tot_count(rid_t t, double count) { profile[t].tot_count += count; }
};

template<class vector_class>
ostream& print_vec(vector_class v, ostream& out=cout)
{
  for(auto vi:v) out<<vi<<" ";
  return out;
}
#endif
