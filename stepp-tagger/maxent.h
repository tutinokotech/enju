/*
 * $Id: maxent.h,v 1.5 2011-05-02 10:53:53 matuzaki Exp $
 */

#ifndef __MAXENT_H_
#define __MAXENT_H_

#include "hashdef.h"
#include "strdic.h"

#include <algorithm>
#include <cassert>
#include <iostream>
#include <list>
#include <map>
#include <string>
#include <vector>

//
// data format for each sample for training/testing
//
struct ME_Sample
{
public:
  ME_Sample() : label("") {};
  ME_Sample(const std::string & l) : label(l) {};
  void set_label(const std::string & l) { label = l; }

  // to add a binary feature
  void add_feature(const std::string & f) {
    features.push_back(f);   
  }

  // to add a real-valued feature
  void add_feature(const std::string & s, const double d) {
    rvfeatures.push_back(std::pair<std::string, double>(s, d)); 
  }

public:
  std::string label;
  std::vector<std::string> features;
  std::vector<std::pair<std::string, double> > rvfeatures;

  // obsolete
  void add_feature(const std::pair<std::string, double> & f) {  
    rvfeatures.push_back(f); // real-valued features
  }
};


//
// for those who want to use load_from_array()
//
typedef struct ME_Model_Data
{
  char * label;
  char * feature;
  double weight;
} ME_Model_Data;


class ME_Model
{
public:

  void add_training_sample(const ME_Sample & s);
  int train(const int cutoff = 0, const double sigma = 0, const double widthfactor = 0);
  std::vector<double> classify(ME_Sample & s) const;
  bool load_from_file(const std::string & filename);
  bool save_to_file(const std::string & filename, const double th) const;
  int num_classes() const { return _num_classes; }
  std::string get_class_label(int i) const { return _label_bag.Str(i); }
  int get_class_id(const std::string & s) const { return _label_bag.Id(s); }
  void get_features(std::list< std::pair< std::pair<std::string, std::string>, double> > & fl);
  void set_heldout(const int h, const int n = 0) { _nheldout = h; _early_stopping_n = n; };
  bool load_from_array(const ME_Model_Data data[]);
  void set_reference_model(const ME_Model & ref_model) { _ref_modelp = &ref_model; };
  void clear();

  ME_Model() {
    _nheldout = 0;
    _early_stopping_n = 0;
    _ref_modelp = NULL;
  }

public:
  // obsolete. just for downward compatibility
  int train(const std::vector<ME_Sample> & train,
            const int cutoff = 0, const double sigma = 0, const double widthfactor = 0);

private:  
  
  struct Sample {
    int label;
    std::vector<int> positive_features;
    std::vector<std::pair<int, double> > rvfeatures;
    std::vector<double> ref_pd; // reference probability distribution
    bool operator<(const Sample & x) const {
      for (unsigned int i = 0; i < positive_features.size(); i++) {
        if (i >= x.positive_features.size()) return false;
        int v0 = positive_features[i];
        int v1 = x.positive_features[i];
        if (v0 < v1) return true;
        if (v0 > v1) return false;
      }
      return false;
    }
  };

  struct ME_Feature
  {
    enum { MAX_LABEL_TYPES = 255 };
      
    //    ME_Feature(const int l, const int f) : _body((l << 24) + f) {
    //      assert(l >= 0 && l < 256);
    //      assert(f >= 0 && f <= 0xffffff);
    //    };
    //    int label() const { return _body >> 24; }
    //    int feature() const { return _body & 0xffffff; }
    ME_Feature(const int l, const int f) : _body((f << 8) + l) {
      assert(l >= 0 && l <= MAX_LABEL_TYPES);
      assert(f >= 0 && f <= 0xffffff);
    };
    int label() const { return _body & 0xff; }
    int feature() const { return _body >> 8; }
    unsigned int body() const { return _body; }
  private:
    unsigned int _body;
  };

  struct ME_FeatureBag
  {
    typedef _HASHMAP<unsigned int, int> map_type;
    map_type mef2id;
    std::vector<ME_Feature> id2mef;
    int Put(const ME_Feature & i) {
      map_type::const_iterator j = mef2id.find(i.body());
      if (j == mef2id.end()) {
        int id = id2mef.size();
        id2mef.push_back(i);
        mef2id[i.body()] = id;
        return id;
      }
      return j->second;
    }
    int Id(const ME_Feature & i) const {
      map_type::const_iterator j = mef2id.find(i.body());
      if (j == mef2id.end()) {
        return -1;
      }
      return j->second;
    }
    ME_Feature Feature(int id) const {
      assert(id >= 0 && id < (int)id2mef.size());
      return id2mef[id];
    }
    int Size() const {
      return id2mef.size();
    }
    void Clear() {
      mef2id.clear();
      id2mef.clear();
    }
  };

  struct MiniStringBag
  {
    typedef _HASHMAP<std::string, int> map_type;
    int _size;
    map_type str2id;
    MiniStringBag() : _size(0) {}
    int Put(const std::string & i) {
      map_type::const_iterator j = str2id.find(i);
      if (j == str2id.end()) {
        int id = _size;
        _size++;
        str2id[i] = id;
        return id;
      }
      return j->second;
    }
    int Id(const std::string & i) const {
      map_type::const_iterator j = str2id.find(i);
      if (j == str2id.end())  return -1;
      return j->second;
    }
    int Size() const { return _size; }
    void Clear() { str2id.clear(); _size = 0; }
    map_type::const_iterator begin() const { return str2id.begin(); }
    map_type::const_iterator end()   const { return str2id.end(); }
  };

  struct StringBag : public MiniStringBag
  {
    std::vector<std::string> id2str;
    int Put(const std::string & i) {
      map_type::const_iterator j = str2id.find(i);
      if (j == str2id.end()) {
        int id = id2str.size();
        id2str.push_back(i);
        str2id[i] = id;
        return id;
      }
      return j->second;
    }
    std::string Str(const int id) const {
      assert(id >= 0 && id < (int)id2str.size());
      return id2str[id];
    }
    int Size() const { return id2str.size(); }
    void Clear() {
      str2id.clear();
      id2str.clear();
    }
  };

  std::vector<Sample> _vs; // vector of training_samples
  StringBag _label_bag;
  //  MiniStringBag _featurename_bag;
  StrDic _featurename_bag;
  double _sigma; // Gaussian prior
  double _inequality_width;
  std::vector<double> _vl;  // vector of lambda
  ME_FeatureBag _fb;
  int _num_classes;
  std::vector<double> _vee;  // empirical expectation
  std::vector<double> _vme;  // empirical expectation
  std::vector< std::vector< int > > _feature2mef;
  std::vector< Sample > _heldout;
  double _train_error;   // current error rate on the training data
  double _heldout_error; // current error rate on the heldout data
  int _nheldout;
  int _early_stopping_n;
  std::vector<double> _vhlogl;
  const ME_Model * _ref_modelp;

  double heldout_likelihood();
  int conditional_probability(const Sample & nbs, std::vector<double> & membp) const;
  int make_feature_bag(const int cutoff);
  int classify(const Sample & nbs, std::vector<double> & membp) const;
  double update_model_expectation();
  int perform_BFGS();
  int perform_GIS(int C);
  void set_ref_dist(Sample & s) const;
  void init_feature2mef();

  double FunctionGradient(const std::vector<double> & x, std::vector<double> & grad);
  static double FunctionGradientWrapper(const std::vector<double> & x, std::vector<double> & grad);
  
};


#endif


/*
 * $Log: not supported by cvs2svn $
 * Revision 1.4  2007/08/10 09:15:56  tsuruoka
 * use StrDic instead of gnu_hash
 *
 * Revision 1.3  2007/06/18 13:04:34  tsuruoka
 * remove blmvm.h
 *
 * Revision 1.2  2007/06/18 09:09:57  tsuruoka
 * replace BLMVM with LBFGS and OWLQN
 *
 * Revision 1.1.1.1  2007/05/15 08:30:35  kyoshida
 * stepp tagger, by Okanohara and Tsuruoka
 *
 * Revision 1.24  2006/08/21 17:30:38  tsuruoka
 * use MAX_LABEL_TYPES
 *
 * Revision 1.23  2006/07/25 13:19:53  tsuruoka
 * sort _vs[]
 *
 * Revision 1.22  2006/07/18 11:13:15  tsuruoka
 * modify comments
 *
 * Revision 1.21  2006/07/18 10:02:15  tsuruoka
 * remove sample2feature[]
 * speed up conditional_probability()
 *
 * Revision 1.20  2006/07/18 05:10:51  tsuruoka
 * add ref_dist
 *
 * Revision 1.19  2005/12/23 10:33:02  tsuruoka
 * support real-valued features
 *
 * Revision 1.18  2005/12/23 09:15:29  tsuruoka
 * modify _train to reduce memory consumption
 *
 * Revision 1.17  2005/10/28 13:02:34  tsuruoka
 * set_heldout(): add default value
 * Feature()
 *
 * Revision 1.16  2005/09/12 13:51:16  tsuruoka
 * Sample: list -> vector
 *
 * Revision 1.15  2005/09/12 13:27:10  tsuruoka
 * add add_training_sample()
 *
 * Revision 1.14  2005/04/27 11:22:27  tsuruoka
 * bugfix
 * ME_Sample: list -> vector
 *
 * Revision 1.13  2005/04/27 10:20:19  tsuruoka
 * MiniStringBag -> StringBag
 *
 * Revision 1.12  2005/04/27 10:00:42  tsuruoka
 * remove tmpfb
 *
 * Revision 1.11  2005/04/26 14:25:53  tsuruoka
 * add MiniStringBag, USE_HASH_MAP
 *
 * Revision 1.10  2004/10/04 05:50:25  tsuruoka
 * add Clear()
 *
 * Revision 1.9  2004/08/09 12:27:21  tsuruoka
 * change messages
 *
 * Revision 1.8  2004/08/04 13:55:19  tsuruoka
 * modify _sample2feature
 *
 * Revision 1.7  2004/07/29 05:51:13  tsuruoka
 * remove modeldata.h
 *
 * Revision 1.6  2004/07/28 13:42:58  tsuruoka
 * add AGIS
 *
 * Revision 1.5  2004/07/28 05:54:14  tsuruoka
 * get_class_name() -> get_class_label()
 * ME_Feature: bugfix
 *
 * Revision 1.4  2004/07/27 16:58:47  tsuruoka
 * modify the interface of classify()
 *
 * Revision 1.3  2004/07/26 17:23:46  tsuruoka
 * _sample2feature: list -> vector
 *
 * Revision 1.2  2004/07/26 15:49:23  tsuruoka
 * modify ME_Feature
 *
 * Revision 1.1  2004/07/26 13:10:55  tsuruoka
 * add files
 *
 * Revision 1.18  2004/07/22 08:34:45  tsuruoka
 * modify _sample2feature[]
 *
 * Revision 1.17  2004/07/21 16:33:01  tsuruoka
 * remove some comments
 *
 */
