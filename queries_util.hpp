#pragma once

#include <iostream>
#include <sstream>

#include "index_types.hpp"
#include "wand_data_compressed.hpp"
#include "util.hpp"
#include "wand_data_raw.hpp"
#include "wand_data.hpp"
#include <math.h>
#include <map>

namespace ds2i {

    typedef uint32_t term_id_type;
    typedef std::vector<term_id_type> term_id_vec;
    typedef std::vector<std::pair<term_id_type, double>> weight_query;


    void normalize_weighted_query(weight_query& query) {
        // Sum of weights
        double wsum = std::accumulate(query.begin(), query.end(), 0, 
                                      [](auto &a, auto &b) {
                                         return a +b.second;
                                       });
        for (size_t i = 0; i < query.size(); ++i) {
          query[i].second = query[i].second/wsum;
        }
    }

    void query_from_ext_to_src(term_id_vec& original, std::unordered_map<term_id_type, term_id_type>& back_map) {
        std::vector<term_id_type>::iterator it = original.begin();
        while (it != original.end()) {
            std::unordered_map<term_id_type, term_id_type>::const_iterator got = back_map.find (*it);
            if ( got == back_map.end() ) {
                //logger() << "Erasing oov " << *it << std::endl; 
                it = original.erase(it);
            }
            else {
                //logger() << "Changing " << *it << " to " << got->second << "." << std::endl; 
                *it = got->second;
                it++;
            }
        }
    }


    void normalize_weighted_query_ext(weight_query& query, std::unordered_map<term_id_type, term_id_type>& back_map) {
        std::vector<std::pair<term_id_type, double>>::iterator it = query.begin();
        
        /* Remove out of vocabulary, and back map the term_ids to the target collection */
        while (it != query.end()) {
            std::unordered_map<term_id_type, term_id_type>::const_iterator got = back_map.find ((*it).first);
            if ( got == back_map.end() ) {
                //logger() << "Erasing oov " << (*it).first << std::endl; 
                it = query.erase(it);
            }
            else {
                //logger() << "Changing " << (*it).first << " to " << got->second << "." << std::endl; 
                (*it).first = got->second;
                it++;
            }
        }

        // Sum of weights
        double wsum = std::accumulate(query.begin(), query.end(), 0, 
                                      [](auto &a, auto &b) {
                                         return a +b.second;
                                       });

        if (wsum > 0.0) {
            for (size_t i = 0; i < query.size(); ++i) {
              query[i].second = query[i].second/wsum;
              //logger() << "Pair: <" << query[i].first << ", " << query[i].second << ">" << std::endl;
            }
        }
    }


    // Assumes original query has unique terms only
    void add_original_query(const double weight, weight_query& w_query, term_id_vec& original) {
        double w_weight = 1-weight;
        double original_term_weight = (1 / (original.size()*1.0f) ) * weight;
        // Update expanded term weights
        for (size_t i = 0; i < w_query.size(); ++i) {
            w_query[i].second = w_query[i].second*w_weight;
        }
        // Add in original terms
        for (size_t i = 0; i < original.size(); i++) {
            w_query.emplace_back(original[i], original_term_weight);
        }
    }

    void dump_weighted_query(weight_query& q) {
        for (size_t i = 0; i < q.size(); ++i) {
          std::cerr << q[i].first << " " << q[i].second << std::endl;
        }
    }

    // Read lex file. Format = <string id, int id, f_t, c_t>
    void read_lexicon (std::ifstream &is, 
                       std::unordered_map<std::string, uint32_t>& lexicon) {
      std::string term;
      size_t id, f_t, c_t;
      while (is >> term >> id >> f_t >> c_t) {
        lexicon.insert({term, id});
      }
      std::cerr << "Lexicon read: " << lexicon.size() << " terms" << std::endl;
    }

    // Read lex file. Format = <string id, int id, f_t, c_t>
    // include reverse mapping
    void read_lexicon (std::ifstream &is, 
                       std::unordered_map<std::string, uint32_t>& lexicon,
                       std::unordered_map<uint32_t, std::string>& reverse) {
      std::string term;
      size_t id, f_t, c_t;
      while (is >> term >> id >> f_t >> c_t) {
        lexicon.insert({term, id});
        reverse.insert({id, term});
      }
      std::cerr << "Lexicon read: " << lexicon.size() << " terms" << std::endl;
    }

    std::vector<term_id_type> parse_query(const std::vector<std::string>& query,
                                          const std::unordered_map<std::string, uint32_t>& lexicon) {
        std::vector<term_id_type> parsed_query;
        for (const auto& term : query) {
            auto f = lexicon.find(term);
            if (f != lexicon.end()) {
                parsed_query.emplace_back(f->second);
            }
        }
        return parsed_query;
    }

    void read_string_query_file(std::map<uint32_t, std::vector<std::string>>& queries, std::ifstream& in) {

        std::string line;
        while (std::getline(in, line)) {
            std::istringstream iline(line);
            uint32_t qid;
            iline >> qid; // Read QID
            std::string s;
            std::vector<std::string> terms;
            while (iline >> s) {
                terms.push_back(s);
            }
            queries.emplace(qid, terms);
        }
    }

    // String query with ID
    bool read_query(term_id_vec &ret, uint32_t &qid, std::unordered_map<std::string, uint32_t>& lex, 
                    std::istream &is = std::cin) {
        ret.clear();
        std::string line;
        if (!std::getline(is, line)) return false;
        std::istringstream iline(line);
        term_id_type term_id;
        iline >> qid; // Read QID
        std::string s_id;
        while (iline >> s_id) {
          auto lexicon_it = lex.find(s_id);
          if (lexicon_it != lex.end()) {
            term_id = lexicon_it->second;
            ret.push_back(term_id);
          }
          else {
            std::cerr << "ERROR: Could not find term '" << s_id << "' in the lexicon." << std::endl;
          }
        }

        return true;
    }


    // int query with ID
    bool read_query(term_id_vec &ret, uint32_t &qid, std::istream &is = std::cin) {
        ret.clear();
        std::string line;
        if (!std::getline(is, line)) return false;
        std::istringstream iline(line);
        term_id_type term_id;
        bool first = true;
        while (iline >> term_id) {
            if (first) {
              qid = term_id;
              first = false;
              continue;
            }
            ret.push_back(term_id);
        }

        return true;
    }



    // int query without QID supplied
    bool read_query(term_id_vec &ret, std::istream &is = std::cin) {
        std::cerr << "WARNING: Read Query [No ID]. This is probably not desired." << std::endl;
        ret.clear();
        std::string line;
        if (!std::getline(is, line)) return false;
        std::istringstream iline(line);
        term_id_type term_id;
        while (iline >> term_id) {
            ret.push_back(term_id);
        }

        return true;
    }


    void remove_duplicate_terms(term_id_vec &terms) {
        std::sort(terms.begin(), terms.end());
        terms.erase(std::unique(terms.begin(), terms.end()), terms.end());
    }


    typedef std::pair<uint64_t, uint64_t> term_freq_pair;
    typedef std::vector<term_freq_pair> term_freq_vec;

    term_freq_vec query_freqs(term_id_vec terms) {
        term_freq_vec query_term_freqs;
        std::sort(terms.begin(), terms.end());
        // count query term frequencies
        for (size_t i = 0; i < terms.size(); ++i) {
            if (i == 0 || terms[i] != terms[i - 1]) {
                query_term_freqs.emplace_back(terms[i], 1);
            } else {
                query_term_freqs.back().second += 1;
            }
        }

        return query_term_freqs;
    }

    struct topk_queue {
        topk_queue(uint64_t k)
                : m_k(k) { }

        topk_queue(const topk_queue &q) : m_q(q.m_q) {
            m_k = q.m_k;
            threshold = q.threshold;
        }

        bool insert(double score) {
            return insert(score, 0);
        }


        bool insert(double score, uint64_t docid) {
            if (m_q.size() < m_k) {
                m_q.push_back(std::make_pair(score, docid));
                std::push_heap(m_q.begin(), m_q.end(), [](std::pair<double, uint64_t> l, std::pair<double, uint64_t> r) {
                    return l.first > r.first;
                });
                threshold = m_q.front().first;
                return true;
            } else {
                if (score > threshold) {
                    std::pop_heap(m_q.begin(), m_q.end(),
                                  [](std::pair<double, uint64_t> l, std::pair<double, uint64_t> r) {
                                      return l.first > r.first;
                                  });
                    m_q.back() = std::make_pair(score, docid);
                    std::push_heap(m_q.begin(), m_q.end(),
                                   [](std::pair<double, uint64_t> l, std::pair<double, uint64_t> r) {
                                       return l.first > r.first;
                                   });
                    threshold = m_q.front().first;
                    return true;
                }
            }
            return false;
        }

        bool would_enter(double score) const {
            return m_q.size() < m_k || score > threshold;
        }

        void finalize() {
            std::sort_heap(m_q.begin(), m_q.end(),
                           [](std::pair<double, uint64_t> l, std::pair<double, uint64_t> r) {
                               return l.first > r.first;
                           });
            size_t size = std::lower_bound(m_q.begin(), m_q.end(), 0, [](std::pair<double, uint64_t> l, double r) {
                return l.first > r;
            }) - m_q.begin();
            m_q.resize(size);
        }

        void sort_docid() {
            std::sort_heap(m_q.begin(), m_q.end(),
                           [](std::pair<double, uint64_t> l, std::pair<double, uint64_t> r) {
                               return l.second < r.second;
                           });
        }

        std::vector<std::pair<double, uint64_t>> const &topk() const {
            return m_q;
        }

        void set_threshold(double t) {
            for (size_t i = 0; i < m_k; ++i) {
                insert(0);
            }
            threshold = t;
        }

        void clear() {
            m_q.clear();
        }

        uint64_t size() {
            return m_k;
        }

        double threshold;
        uint64_t m_k;
        std::vector<std::pair<double, uint64_t>> m_q;
    };

}
