#include <matrixPreparation.h>
namespace po = boost::program_options;
using namespace std::chrono;
using namespace std;
using namespace arma;




std::set<NetworKit::node> getdescNodes(NetworKit::Graph& go, NetworKit::node& n){
    std::set<NetworKit::node> descendants;
    NetworKit::Graph::NeighborRange<true> childIT= go.inNeighborRange(n);

    for(NetworKit::Graph::NeighborIterator it = childIT.begin(); it!=childIT.end();it++){
        NetworKit::node nd = it.operator*();
        descendants.insert(nd);
        std::set<NetworKit::node> vd = ::getdescNodes(go,nd);
        descendants.insert(vd.begin(),vd.end());
    }
    return descendants;
};


std::vector<GOTerm> getdescendants(NetworKit::Graph& go, NetworKit::node& n, std::map<NetworKit::node,GOTerm>& idx2goterm){
    std::vector<GOTerm> descendants;
    std::set<NetworKit::node> setNodes = ::getdescNodes(go,n);
    for(NetworKit::node n : setNodes){
        descendants.push_back(idx2goterm.at(n));
    }
    return descendants;
};

std::set<NetworKit::node> getancNodes( NetworKit::node& n ,std::map<NetworKit::node,GOTerm>& idx2goterm){
    std::set<NetworKit::node> ancestors;

    for(NetworKit::node nd : idx2goterm.at(n).get_parents()){

        ancestors.insert(nd);
        std::set<NetworKit::node> vd = ::getancNodes(nd,idx2goterm);
        ancestors.insert(vd.begin(),vd.end());
    }
    return ancestors;
};


std::vector<GOTerm> getancestors(NetworKit::node& n, std::map<NetworKit::node,GOTerm>& idx2goterm){
    std::vector<GOTerm> descendants;
    for(NetworKit::node n : ::getancNodes(n,idx2goterm)){
        descendants.push_back(idx2goterm.at(n));
    }
    return descendants;
};



// Reading obo file
// This function allow to read a go file and fulfill three variables:
//      idx2goterm - a map with the node index as key and the GOTerm class as
//      go - Graph of Gene Ontology
//      gotermsIDidx - a map with the GOID as key and the node index as value
void reader(std::string go_file, std::map<NetworKit::node,GOTerm>& idx2goterm,NetworKit::Graph& go, std::map<std::string,NetworKit::node>& gotermsID2idx, std::map<std::string,Root>& namespace2root, int& threads) {
  std::string line;
  std::ifstream myfile (go_file);
  std::string delimiter = ":";
  std::string token = "";
  bool isTerm = false;
  std::vector<std::string> termInfo;
  bool obsolete = false;
  std::map<NetworKit::node,std::vector<std::string>> node2parent;
  std::vector<std::string> parentvector;
  std::vector<NetworKit::node> rootID;

  if (myfile.is_open())
  {
    while ( getline (myfile,line) )
    {
      if(line==""){
        token=line;
        if(isTerm==true && obsolete!=1){

            NetworKit::node n = go.addNode();
            GOTerm term(termInfo, n);
            idx2goterm.insert(std::pair<NetworKit::node,GOTerm>(n,term));

            gotermsID2idx.insert(std::pair<std::string, NetworKit::node>(termInfo.at(0),n));

            if( parentvector.size() == 0){
                rootID.push_back(n);
            }

            node2parent.insert(std::pair<NetworKit::node&,std::vector<std::string> >(n,parentvector));
          }
          isTerm=0;
          parentvector.clear();
          termInfo.clear();
          isTerm=false;
          obsolete=false;
        }


      if(line=="[Term]"){
        isTerm=1;
        token=line;


      }

      if(token=="[Term]") {

        std::string key = line.substr(0, line.find(delimiter));
        std::string value = line.substr(line.find(delimiter)+2,line.size());
        // cout << key <<"-:-"<<value<<"\n";
        if(key=="id"){
          termInfo.push_back(value);
        }else if(key=="name"){
          termInfo.push_back(value);
        }else if(key=="is_a"){
          std::string delimiter = " ! ";
          std::string rel = value.substr(0, value.find(delimiter));
          parentvector.push_back(rel);

        }else if(key=="namespace"){
         termInfo.push_back(value);
        }
        else if(key=="is_obsolete"){
          if(value=="true"){
            obsolete=true;
          }
    }


      }
    }
       myfile.close();

    std::map<NetworKit::node, std::vector<std::string>>::iterator it;
    for ( it = node2parent.begin(); it != node2parent.end(); it++ )
      {

        for(std::string v : it->second){

            go.addEdge(it->first,gotermsID2idx.at(v),ew_default);
            idx2goterm.at(it->first).set_parent(gotermsID2idx.at(v));
            idx2goterm.at(gotermsID2idx.at(v)).set_child(it->first);

        }
    }
    for(NetworKit::node n : rootID){
        long nd = static_cast<long>(::getdescendants(go,n,idx2goterm).size());
        GOTerm gt = idx2goterm.at(n);
        Root r(gt.id,nd);
        namespace2root.insert(std::pair<std::string,Root>(gt.name,r));
    }

std::map<NetworKit::node,GOTerm>::iterator iter=idx2goterm.begin();
#pragma omp parallel num_threads(threads)
{

    for(;iter!=idx2goterm.end();iter++){
        NetworKit::node n = iter->first;
        GOTerm& gt = iter->second;
        double ic = 1 - log10(static_cast<double>(::getdescNodes(go,n).size())+1)/
                        log10(static_cast<double>(namespace2root.at(gt.top).ndescendants));
        gt.set_IC(ic);

    }

}

  }else{
    std::cout<<"Impossible to open\n";

  }
}




Mat<double> createPPIfromFile(string ppi_file, vector<string>& geneNodes){
ifstream myfile(ppi_file);
string line;
set<string> nodes;
vector<string> wedges;

if (myfile.is_open())
{
  while ( getline (myfile,line) )
  {

      vector<string> results;

      boost::algorithm::split(results, line, boost::is_any_of("\t "));

      if(results.at(0)!="Source"){

          string s = results.at(0).substr(0, results.at(0).find("."));
          string t = results.at(1).substr(0, results.at(1).find("."));

      nodes.insert(s);
      nodes.insert(t);
      string l = s + " " + t + " " + results.at(2);
      wedges.push_back(l);
      }
}
   myfile.close();

map<string, size_t> str2id;
size_t id =0;
for(string n: nodes){
    geneNodes.push_back(n);
    str2id.insert(pair<string,size_t>(n,id));
    id++;
}

  Mat<double> ppi(geneNodes.size(),geneNodes.size(),fill::zeros);

   for(string e: wedges){
       vector<string> results;
       boost::algorithm::split(results, e, boost::is_any_of(" "));
       double ew = ::atof(results.at(2).c_str());

       ppi.at(str2id.at(results.at(0)),str2id.at(results.at(1)))= ew;
        ppi.at(str2id.at(results.at(1)),str2id.at(results.at(0)))= ew;
   }

   return ppi;

}else{
  cout<<"Impossible to open\n";

}
}


/*
 * readAnnotation function read an annotation file to create a Annotation obj. This annotation obj associate a gene to the terms in the file and all their ancestors. For now, take so much
 * time, it has to be improved.
 */
void readAnnotation(string fileAnnotation, Annotation& annotation, map<string, NetworKit::node>& go2idx, map<NetworKit::node,GOTerm>& idx2goterm, vector<NetworKit::node>& selectedGOs){
    ifstream myfile (fileAnnotation);
    string line;
    set<NetworKit::node> setGOs;

    map<string, set<NetworKit::node>> t2anc;
    map<NetworKit::node,GOTerm>::iterator gtIT;
    for(gtIT = idx2goterm.begin(); gtIT != idx2goterm.end(); gtIT++){
        NetworKit::node n(gtIT->first);
        string& id = gtIT->second.id;
        t2anc.insert(pair<string,set<NetworKit::node>>(id,::getancNodes(n,idx2goterm)));
    }



    if (myfile.is_open())
    {
      while ( getline (myfile,line) )
      {

          vector<string> results;

          boost::algorithm::split(results, line, boost::is_any_of("\t "));

          if(go2idx.find(results.at(1))!=go2idx.end()){
          annotation.setAnnotation(results.at(0),go2idx.at(results.at(1)));

          setGOs.insert(go2idx.at(results.at(1)));
          for(NetworKit::node n : t2anc.at(results.at(1)))
          {
              annotation.setAnnotation(results.at(0),n);
              setGOs.insert(n);
          }
          }

    }
       myfile.close();

       for(NetworKit::node n : setGOs){
           selectedGOs.push_back(n);
       }

    }else{
      cout<<"Impossible to open\n";

    }
}

Mat<double> computeGxTMatrix(vector<string>& geneNodes, vector<NetworKit::node>& selectedGOs, Annotation& annotation){
     Mat<double> selAssoMatrix(geneNodes.size(),selectedGOs.size(),fill::zeros);
    for(size_t g = 0; g< geneNodes.size(); g++){


    for(NetworKit::node t : annotation.getGOTerms(geneNodes.at(g))){
        auto n = find(selectedGOs.begin(),selectedGOs.end(), t);
        auto ti = static_cast<size_t>(distance(selectedGOs.begin(), n));
        selAssoMatrix.at(g,ti) = 1;
    }
    }
    return selAssoMatrix;
}

double icSum(std::vector<NetworKit::node>& childrens, std::map<NetworKit::node, GOTerm>& idx2goTerm){
double sumChildsIC =0.;

for(NetworKit::node c :childrens ){
    sumChildsIC = sumChildsIC +idx2goTerm.at(c).get_IC();
}
 return sumChildsIC;
}

double transitionalProb(NetworKit::node& s, NetworKit::node& t, Annotation& annotation, map<NetworKit::node, GOTerm>& idx2goTerm, double icSum){

    return static_cast<double>(annotation.getGenes(s).size())/static_cast<double>(annotation.getGenes(t).size()) +
           idx2goTerm.at(s).get_IC()/icSum;

}

double transitionalProbSUM(NetworKit::node& t, Annotation& annotation, map<NetworKit::node, GOTerm>& idx2goTerm, double icSum){

    double sumChildsTP =0.;

    for(NetworKit::node c : idx2goTerm.at(t).get_childrens()){
        sumChildsTP = sumChildsTP + transitionalProb(c,t,annotation, idx2goTerm, icSum);
    }

    return sumChildsTP;

}



/*
 * computeTPMatrix function computes a transational probability matrix. This matrix is T x T where T includes the terms (and their ancestors) annotating the
 * genes of the chosen organism. This matrix only have a value if a term i in T is associated to a term j in T.
 */
Mat<double> computeTPMatrix(Annotation& annotation, map<NetworKit::node, GOTerm>& idx2goTerm, vector<NetworKit::node>& selectedGOs){

    Mat<double> TPMatrix(selectedGOs.size(),selectedGOs.size(),fill::zeros);
    size_t t;
    map<NetworKit::node,double> t2TPsum;
    map<NetworKit::node,double> t2ICchilds;


    for(t =0; t< selectedGOs.size(); t++){
        double d = icSum( idx2goTerm.at(selectedGOs.at(t)).get_childrens(), idx2goTerm);
        t2TPsum.insert(pair<NetworKit::node,double>(selectedGOs.at(t),transitionalProbSUM(selectedGOs.at(t),annotation,idx2goTerm,d)));
        t2ICchilds.insert(pair<NetworKit::node,double>(selectedGOs.at(t),d));
    }
    for(t =0; t< selectedGOs.size(); t++){

        for(NetworKit::node cn : idx2goTerm.at(selectedGOs.at(t)).get_childrens()){
            vector<NetworKit::node>::iterator it = find(selectedGOs.begin(),selectedGOs.end(),cn);
            if(it!= selectedGOs.end()){
               size_t pos  =  static_cast<size_t>(distance(selectedGOs.begin(),it));
               double tp = transitionalProb(cn,selectedGOs.at(t), annotation, idx2goTerm,t2ICchilds.at(selectedGOs.at(t)));

               double tpD = tp/t2TPsum.at(selectedGOs.at(t));
               TPMatrix.at(t,pos) = tpD;
            }
        }

    }

    return TPMatrix;
}

double TO(vector<NetworKit::node> setGO1, vector<NetworKit::node> setGO2, double& threshold){

    sort(setGO1.begin(),setGO1.end());
    sort(setGO2.begin(),setGO2.end());
    std::vector<int> v_intersection;


        std::set_intersection(setGO1.begin(), setGO1.end(),
                              setGO2.begin(), setGO2.end(),
                              std::back_inserter(v_intersection));
       double ss = static_cast<double>(v_intersection.size())/static_cast<double>((max(setGO1.size(),setGO2.size())));
  if(ss>=threshold){
    return ss ;
  }else{
      return 0.;
  }
}

Mat<double> computeSS(Annotation& annotation,  vector<string>& geneNodes,int threads, double& threshold){


    Mat<double> ssMatrix(geneNodes.size(),geneNodes.size(),fill::zeros);
    size_t i;
    size_t j;


#pragma omp parallel shared(ssMatrix) num_threads(threads)
{

#pragma omp for private(i,j)
        for(i =0; i < geneNodes.size();i++){
        ssMatrix.at(i,i) = 1;

        for(j=i+1;j < geneNodes.size();j++){
           ssMatrix.at(i,j) =
                   ssMatrix.at(j,i) =
                   TO(annotation.getGOTerms(geneNodes.at(i)),annotation.getGOTerms(geneNodes.at(j)),threshold);


        }

    }
  }
    return ssMatrix;
}


Mat<double> normilizeMatrix(Mat<double>& matrix){
    rowvec d2 = sum(matrix,0);
    size_t s = d2.size();
    size_t i;
    Mat<double> nMatrix = matrix;
    for(i = 0; i < s; i++){
        if(d2.at(i)>0.){
            double x = 1./sqrt(d2.at(i));
            for(size_t j = i; j<s;j++)
            {
                if(d2.at(j)>0.){
                    double y = 1./sqrt(d2.at(j));
                    nMatrix(i,j) = nMatrix(j,i) = nMatrix(i,j)*(x*y);
                }
            }
        }
    }


    return nMatrix;
}

void exportVector(string filename, vector<NetworKit::node>& v){

    stringstream ss;
    for(size_t i = 0; i < v.size(); ++i)
    {
      if(i != 0)
        ss << ",";
      ss << v[i];
    }
    ofstream myfile;
      myfile.open (filename);
      myfile << ss.str();
      myfile.close();

}

void exportVector(string filename, vector<string>& v){

    stringstream ss;
    for(size_t i = 0; i < v.size(); ++i)
    {
      if(i != 0)
        ss << ",";
      ss << v[i];
    }
    ofstream myfile;
      myfile.open (filename);
      myfile << ss.str();
      myfile.close();

}


void preComputeMatrix(int threads, double threshold_ss, string obofile, string goafile, string networkfile, string outFolder){
    vector<string> geneNodes;
    Annotation annotation;
    vector<NetworKit::node> selectedGOs;
    /*
     * Create Matrix
     */
// ex netFile = "/home/aaron/git/Umea/GNI_predictors/ext-data/edgelist.txt"
     auto start = high_resolution_clock::now();
        Mat<double> ppi = createPPIfromFile(networkfile,geneNodes);
        auto stop = high_resolution_clock::now();
        auto duration = duration_cast<milliseconds>(stop - start);
        cout <<"To read the network file took " <<duration.count()<<" milliseconds." << endl;
       //colvec c = sum(ppi,1);
       //cout<<"vector sum columns, position 0 = "<< c.at(0)<<endl;
       // READ GO file
       NetworKit::Graph go(0,true,true); // the first true is used to consider the graph as weigthed and the second true to consider the graph directed
       map<NetworKit::node,GOTerm> idx2goterm;
       map<string,NetworKit::node> gotermsID2idx;
       map<string,Root> namespace2root;
       start = high_resolution_clock::now();
// ex obofile = "/home/aaron/git/Umea/GNI_predictors/data/go.obo"
       ::reader(obofile,idx2goterm, go, gotermsID2idx,namespace2root,threads);
       stop = high_resolution_clock::now();
       duration = duration_cast<milliseconds>(stop - start);
       cout <<"To read the Gene Ontology took " <<duration.count()<<" milliseconds." << endl;
       // READ GOA file
       start = high_resolution_clock::now();
// ex annotFile = "/home/aaron/git/Umea/GNI_predictors/ext-data/Potra_gene_go.tsv"
       readAnnotation(goafile, annotation, gotermsID2idx,idx2goterm, selectedGOs);
       stop = high_resolution_clock::now();
       duration = duration_cast<seconds>(stop - start);
       cout <<"To read the Annotation file took "  <<duration.count()<<" milliseconds." << endl;
        // Create association Matrix
       start = high_resolution_clock::now();
       Mat<double> selAssoMatrix = computeGxTMatrix(geneNodes,selectedGOs, annotation);
       stop = high_resolution_clock::now();
       duration = duration_cast<milliseconds>(stop - start);
       cout << "To create the gene2GOterm association matrix took " <<duration.count()<<" milliseconds." << endl;
        // Create the transitional probability matrix
       start = high_resolution_clock::now();
       Mat<double> TPMatrix =  computeTPMatrix(annotation,idx2goterm,selectedGOs);
       stop = high_resolution_clock::now();
       duration = duration_cast<milliseconds>(stop - start);
       cout << "To create the transitional probability matrix took "<< duration.count()<<" milliseconds." << endl;
       // Create the semantic ppi
       start = high_resolution_clock::now();
       Mat<double> ssMatrix = computeSS(annotation, geneNodes,threads,threshold_ss);
       stop = high_resolution_clock::now();
       auto duration2 = duration_cast<seconds>(stop - start);
       cout <<"To create the semantic ppi took " <<duration2.count()<<" seconds." << endl;

   /*
    * NORMALIZE
    */
       //PPI
       start = high_resolution_clock::now();
       Mat<double> nPpi = normilizeMatrix(ppi);
       stop = high_resolution_clock::now();
       duration2 = duration_cast<seconds>(stop - start);
       cout <<"To normalize the PPI matrix took "<< duration2.count()<<" seconds." << endl;
       // Semantic PPI
       start = high_resolution_clock::now();
       Mat<double> nSSPpi = normilizeMatrix(ssMatrix);
       stop = high_resolution_clock::now();
       duration = duration_cast<seconds>(stop - start);
       cout <<"To normalize the semantic PPI matrix took "<< duration.count()<<" seconds." << endl;
       // Translational probability
       start = high_resolution_clock::now();
       Mat<double> hybrid = nPpi + nSSPpi;
       /*
        * Save Matrix
        */
      hybrid.save( outFolder+"/hybrid.bin");
      nPpi.save( outFolder+"/nPpi.bin");
      nSSPpi.save( outFolder+"/nSSPpi.bin");
      TPMatrix.save(outFolder+"/TPMatrix.bin");
      selAssoMatrix.save(outFolder+"/annotationMatrix.bin");
      /*
       * Save vectors
       */
      exportVector(outFolder+"/genes.txt",geneNodes);

      exportVector(outFolder+"/GOterms.txt",selectedGOs);
}



int matrixPreparation(int ac, char* av[])
  {
    int threads;
    double threshold_ss= 0.6;
    string outFolder;
    string oboFile;
    string goaFile;
    string networkFile;

    po::options_description desc("MatrixPreparation options");
            desc.add_options()
                    ("help,h", "produce help message")
                    ("num_threads,T", po::value<int>(&threads) ->default_value(1), "set number of threads to parallize")
                    ("ss_threshold,S", po::value<double>(&threshold_ss)->default_value(static_cast<double>(0.6)), "Set a semantic similarity value threshold to filter links between genes/proteins with low similarity")
                    ("goFile,G",po::value<string>(&oboFile),"Set the path of gene ontology file [OBO format]")
                    ("annotationFile,A",po::value<string>(&goaFile),"Set the path of annotation File [tsv format]")
                    ("networkInput,N",po::value<string>(&networkFile), "network file")
                    ("outFolder,O",po::value<string>(&outFolder)->default_value("./results"), "output folder path to provide the results")
                    ;

            po::variables_map vm;
            po::store(po::parse_command_line(ac, av, desc), vm);
            po::notify(vm);
             po::store(po::command_line_parser(ac, av).
                       options(desc).run(), vm);
          //  cout<<ac<<endl;
             if (vm.count("help") || ac == 2)
             {
               std::cerr << desc << '\n';
               return 1;
             }
            vector<string> err;
            bool flag = false;
             if(!vm.count("goFile")){
                 err.push_back("[ERROR] Please set the gene ontology file [OBO format] \n");
                 flag=true;
             }

             if(!vm.count("annotationFile")){
                 err.push_back("[ERROR] Please set the annotation File [tsv format] \n");
                 flag=true;
             }
             if(!vm.count("networkInput")){
                 err.push_back("[ERROR] Please set the network File\n");
                 flag=true;
             }


             if(flag){
                 for(string s : err){
                     cerr<<s;
                 }
                 cerr<< desc<<endl;
                 return 1;
             }

             if (!boost::filesystem::exists(oboFile)){
                 err.push_back("[ERROR] Please use a correct path including the obo file.");
                 flag=true;
             }
             if (!boost::filesystem::exists(goaFile)){
                 err.push_back("[ERROR] Please use a correct path including the goa file.");
                 flag=true;
             }
             if (!boost::filesystem::exists(networkFile)){
                 err.push_back("[ERROR] Please use a correct path including the network file.");
                 flag=true;
             }

             if(flag){
                 for(string s : err){
                     cerr<<s;
                 }
                 cerr<< desc<<endl;
                 return 1;
             }


            if (!boost::filesystem::exists(outFolder)){
               boost::filesystem::create_directory(outFolder);
              }

     preComputeMatrix(threads,threshold_ss,oboFile,goaFile,networkFile,outFolder);

  return 0;
  }
