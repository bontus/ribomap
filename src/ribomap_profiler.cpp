#include <vector>
#include <array>
#include <string>
#include <algorithm>
#include <cassert>
#include <cmath>
#include <numeric>
#include <stdexcept>
#include <thread>
#include <functional>
#include <iostream>
#include <limits> 

#include "reference_info_builder.hpp"
#include "ribomap_profiler.hpp"
#include "bam_parser.hpp"

/***
 * class ribo_profile: 
 * assign reads according to transcript abundance
 */
ribo_profile::ribo_profile(const transcript_info& tinfo, const char* fname, const string& filetype, double abundance_cutoff): nonzero_abundance_vec(boost::dynamic_bitset<>(tinfo.total_count())), total_read_count(0)
{
  if (filetype=="sailfish")
    sailfish_parser(tinfo,fname,abundance_cutoff);
  else if (filetype=="cufflinks")
    cufflinks_parser(tinfo,fname,abundance_cutoff);
  else if (filetype=="express")
    express_parser(tinfo,fname,abundance_cutoff);
  else {
    std::cerr<<"ERROR: transcript abundance estimation file type "<<filetype<<" not supported!\n";
    exit(1);
  }
}

void ribo_profile::sailfish_parser(const transcript_info& tinfo, const char* sf_fname, double abundance_cutoff)
{
  rid_t pid(0);
  ifstream ifile(sf_fname);
  while(ifile.peek() == '#'){
    string line;
    getline(ifile,line);
  }
  string word, tid;
  int i(0);
  double tpm(0), total_abundance(0);
  while(ifile >> word) {
    ++i;
    // 1st column: transcript ID
    if (i==1) {
      // parse Gencode transcript ID to get the actural ID
      size_t id(word.find('|'));
      if (id!= word.npos)
	tid = word.substr(0,id);
      else
	tid = word;
    }
    // 3rd column: transcript abundance (tpm: transcript per million)
    else if (i==3) {
      try { tpm = std::stod(word); }
      catch (const std::out_of_range& oor) { tpm = 0; }
      ifile.ignore(numeric_limits<streamsize>::max(), '\n');
      i = 0;
      if (tpm < abundance_cutoff) continue;
      rid_t rid(tinfo.get_refID(tid));
      int plen(tinfo.cds_pep_len(rid));
      // total number of (t,i) fragments: #transcript x plen
      total_abundance += tpm * plen;
      // initialize profile list
      vector<double> count(plen,0);
      profile.emplace_back(tprofile{0, count, tpm});
      refID2pID[rid] = pid++;
      include_abundant_transcript(rid);
    }
  }
  ifile.close();
  // normalize abundance
  for (size_t t = 0; t!=profile.size(); ++t)
    profile[t].tot_abundance /= total_abundance;
}

void ribo_profile::cufflinks_parser(const transcript_info& tinfo, const char* cl_fname, double abundance_cutoff)
{
  rid_t pid(0);
  ifstream ifile(cl_fname);
  // first line is header description, disgarded. 
  ifile.ignore(numeric_limits<streamsize>::max(), '\n');
  string word, tid;
  int i(0);
  double fpkm(0), total_abundance(0);
  while (ifile >> word) {
    ++i;
    // 1st column: transcript ID
    if (i==1) {
      // parse Gencode transcript ID to get the actural ID
      size_t id(word.find('|'));
      if (id!= word.npos)
	tid = word.substr(0,id);
      else
	tid = word;
    }
    // 10th column: transcript abundance (fpkm)
    else if (i==10) {
      try { fpkm = std::stod(word); }
      catch (const std::out_of_range& oor) { fpkm = 0; }
      ifile.ignore(numeric_limits<streamsize>::max(), '\n');
      i = 0;
      if (fpkm < abundance_cutoff) continue;
      rid_t rid(tinfo.get_refID(tid));
      if (rid == tinfo.total_count()) continue;
      int plen(tinfo.cds_pep_len(rid));
      total_abundance += fpkm;
      // initialize profile list
      vector<double> count(plen,0);
      profile.emplace_back(tprofile{0, count, fpkm});
      refID2pID[rid] = pid++;
      include_abundant_transcript(rid);
    }
  }
  ifile.close();
  // normalize abundance
  for (size_t t = 0; t!=profile.size(); ++t)
    profile[t].tot_abundance /= total_abundance;
}

void ribo_profile::express_parser(const transcript_info& tinfo, const char* ep_fname, double abundance_cutoff)
{
  rid_t pid(0);
  ifstream ifile(ep_fname);
  // first line is header description, disgarded. 
  ifile.ignore(numeric_limits<streamsize>::max(), '\n');
  string word, tid;
  int i(0);
  double fpkm(0), total_abundance(0);
  while (ifile >> word) {
    ++i;
    // 1st column: transcript ID
    if (i==2) {
      // parse Gencode transcript ID to get the actural ID
      size_t id(word.find('|'));
      if (id!= word.npos)
	tid = word.substr(0,id);
      else
	tid = word;
    }
    // 11th column: transcript abundance
    else if (i==11) {
      try { fpkm = std::stod(word); }
      catch (const std::out_of_range& oor) { fpkm = 0; }
      ifile.ignore(numeric_limits<streamsize>::max(), '\n');
      i = 0;
      if (fpkm < abundance_cutoff) continue;
      rid_t rid(tinfo.get_refID(tid));
      if (rid == tinfo.total_count()) continue;
      int plen(tinfo.cds_pep_len(rid));
      total_abundance += fpkm;
      // initialize profile list
      vector<double> count(plen,0);
      profile.emplace_back(tprofile{0, count, fpkm});
      refID2pID[rid] = pid++;
      include_abundant_transcript(rid);
    }
  }
  ifile.close();
  // normalize abundance
  for (size_t t = 0; t!=profile.size(); ++t)
    profile[t].tot_abundance /= total_abundance;
}

bool ribo_profile::initialize_read_count(const fp_list_t&  fp_codon_list, bool normalize)
{
  for (auto r: fp_codon_list) {
    vector<read_loc_count> loc_count_list;
    double tot_count(r.count);
    for (auto pos: r.al_loci) {
      rid_t refID(pos.refID); //transcript index
      rid_t t(get_transcript_index(refID));
      int start(pos.start), stop(pos.stop);
      if (start<0 or stop>len(t)) {
	cout<<"profile index out of bound! readID:"<<*r.seqs.begin()<<" ";
	cout<<r.al_loci.size()<<" "<<refID<<" "<<start<<"-"<<stop<<" "<<len(t)<<endl;
	continue;
      }
      for (rid_t i=start; i!=stop; ++i)
	loc_count_list.emplace_back(read_loc_count{t,i,tot_count});
    }// for codon range list
    read_count_list.emplace_back(read_count{tot_count,loc_count_list});
    total_read_count += tot_count;
  }// for r
  if (normalize)
    for (auto& r: read_count_list)
      r.tot_count /= total_read_count;
  return false;
}

vector<rid_t> ribo_profile::get_expressed_transcript_ids() const
{
  vector<rid_t> refID_vec(refID2pID.size());
  for (auto p: refID2pID)
    refID_vec[p.second] = p.first;
  return refID_vec;
}

// bowtie-best: reads only map to one place, no need to build read_count_list
bool ribo_profile::single_map_read_count(const fp_list_t& fp_codon_list)
{
  for (auto r: fp_codon_list) {
    auto& pos(r.al_loci[0]);
    rid_t refID(pos.refID); //transcript index in ribo_profile.profile
    rid_t t(get_transcript_index(refID));
    // sanity check for transcript index
    if (t>number_of_transcripts()-1){
      cout<<"transcript index out of bound! readID:"<<*r.seqs.begin()<<" ";
      cout<<t<<" "<<number_of_transcripts()<<endl;
    }
    int start(pos.start), stop(pos.stop);
    // sanity check for profile index
    if (start<0 or stop>len(t)) {
      cout<<"profile index out of bound! readID:"<<*r.seqs.begin()<<" ";
      cout<<r.al_loci.size()<<" "<<refID<<" "<<start<<"-"<<stop<<" "<<len(t)<<endl;
    }
    for (rid_t i=start; i!=stop; ++i){
      add_read_count(t,i,r.count);
      add_tot_count(t,r.count);
    }
  }// for r
  return false;
}

// assign reads based on transcript abundance only 
bool ribo_profile::assign_reads()
{
  for (auto& r: read_count_list) {
    double current_read_abd(0);
    // round 1: get expected values
    // numerators stored in read_local_count.count
    // denominators stored in current_read_abd
    for (auto& p: r.loc_count_list) {
      p.count = get_tot_abundance(p.tid);
      current_read_abd += p.count;
    }//round 1
    if (current_read_abd == 0) continue;
    // round 2: update counts
    for (auto& p: r.loc_count_list)
      p.count *= r.tot_count/current_read_abd;
  }// for r
  return false;
}

void ribo_profile::reset_read_count()
{
  for (auto& t: profile) {
    t.tot_count = 0;
    fill_n(t.count.begin(), t.count.size(), 0);
  }
}

void ribo_profile::update_count_profile()
{
  reset_read_count();
  for (auto r: read_count_list) {
    for (auto p: r.loc_count_list) {
      add_read_count(p.tid,p.loc,p.count);
      add_tot_count(p.tid,p.count);
    }
  }
}
