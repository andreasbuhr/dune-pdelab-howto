// -*- tab-width: 4; indent-tabs-mode: nil -*-
/** \file 
    \brief High-level test with Poisson equation
*/
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif
#include<iostream>
#include<vector>
#include<map>
#include<dune/common/parallel/mpihelper.hh>
#include<dune/common/exceptions.hh>
#include<dune/common/fvector.hh>
#include<dune/common/static_assert.hh>
#include<dune/common/timer.hh>
#include<dune/grid/yaspgrid.hh>
#include<dune/istl/bvector.hh>
#include<dune/istl/operators.hh>
#include<dune/istl/solvers.hh>
#include<dune/istl/preconditioners.hh>
#include<dune/istl/io.hh>
#include<dune/istl/paamg/amg.hh>
#include<dune/istl/superlu.hh>
#include<dune/grid/io/file/vtk/subsamplingvtkwriter.hh>

#include<dune/pdelab/finiteelementmap/monomfem.hh>
#include<dune/pdelab/finiteelementmap/opbfem.hh>
#include<dune/pdelab/finiteelementmap/qkdg.hh>
#include<dune/pdelab/finiteelementmap/pkfem.hh>
#include<dune/pdelab/finiteelementmap/p0fem.hh>
#include<dune/pdelab/constraints/common/constraints.hh>
#include<dune/pdelab/constraints/conforming.hh>
#include<dune/pdelab/constraints/p0.hh>
#include<dune/pdelab/gridfunctionspace/gridfunctionspace.hh>
#include<dune/pdelab/gridfunctionspace/gridfunctionspaceutilities.hh>
#include<dune/pdelab/gridfunctionspace/interpolate.hh>
#include<dune/pdelab/common/function.hh>
#include<dune/pdelab/common/functionutilities.hh>
#include<dune/pdelab/common/vtkexport.hh>
#include<dune/pdelab/backend/istlvectorbackend.hh>
#include<dune/pdelab/backend/istlmatrixbackend.hh>
#include<dune/pdelab/backend/istlsolverbackend.hh>
#include<dune/pdelab/localoperator/convectiondiffusionparameter.hh>
#include<dune/pdelab/localoperator/convectiondiffusiondg.hh>
#include<dune/pdelab/localoperator/diffusionccfv.hh>
#include<dune/pdelab/stationary/linearproblem.hh>
#include<dune/pdelab/gridoperator/gridoperator.hh>

#include"problemA.hh"


#include <dune/common/memory/blocked_allocator.hh>

#include <dune/pdelab/backend/istl/blockvectorbackend.hh>
#include <dune/pdelab/backend/istl/bellmatrixbackend.hh>

#include <dune/pdelab/ordering/dgredblack.hh>
#include <dune/pdelab/ordering/permutedordering.hh>

#include<dune/istl/preconditioners/sequentialjacobi.hh>
#include<dune/istl/preconditioners/sequentialsor.hh>

const bool graphics = true;

/*
  With this class you can specify how to distribute the total number of
  processes to the YASP grid by passing a vector of type 
  Dune::FieldVector<int,dim> to the constructor.
*/
template<int dim, class iTupel>
class YaspPartition : public Dune::YLoadBalance<dim>
{
private:
  const iTupel& yasppartitions;

public:
  //constructor:
  YaspPartition( const iTupel& yasppartitions_ )
    : yasppartitions( yasppartitions_ )
  {
  }

  void loadbalance (const iTupel& size, int P, iTupel& dims) const
  {
    dims = yasppartitions;
  }
};




template<class GV> 
void test (const GV& gv)
{
/*
  typedef typename GV::Grid::ctype DF;
  typedef double RF;
  const int dim = GV::dimension;
  Dune::Timer watch;

  // instantiate finite element maps
  typedef Dune::PDELab::P0LocalFiniteElementMap<DF,RF,dim> FEM;
  FEM fem(Dune::GeometryType(Dune::GeometryType::cube,dim)); // works only for cubes
  
  // make function space
  typedef Dune::PDELab::ISTLVectorBackend<> VBE;
  typedef Dune::PDELab::GridFunctionSpace<GV,FEM,Dune::PDELab::P0ParallelConstraints,VBE> GFS;
  watch.reset();
  GFS gfs(gv,fem);

  // local operator
  watch.reset();
  typedef k_A<GV,RF> KType;
  KType k(gv);
  typedef A0_A<GV,RF> A0Type;
  A0Type a0(gv);
  typedef F_A<GV,RF> FType;
  FType f(gv);
  typedef B_A<GV> BType;
  BType b(gv);
  typedef J_A<GV,RF> JType;
  JType j(gv);
  typedef G_A<GV,RF> GType;
  GType g(gv);
  typedef Dune::PDELab::DiffusionCCFV<KType,A0Type,FType,BType,JType,GType> LOP;
  LOP lop(k,a0,f,b,j,g);

  // make constraints map and initialize it from a function
  typedef typename GFS::template ConstraintsContainer<RF>::Type CC;
  CC cc;
  cc.clear();
  Dune::PDELab::constraints(g,gfs,cc,false);

  // grid operator
  typedef Dune::PDELab::ISTLMatrixBackend MBE;
  typedef Dune::PDELab::GridOperator<GFS,GFS,LOP,MBE,RF,RF,RF,CC,CC> GO;
  GO go(gfs,cc,gfs,cc,lop);

  // make coefficent Vector and initialize it from a function
  typedef typename GO::Traits::Domain V;
  V x(gfs);
  x = 0.0;
  Dune::PDELab::interpolate(g,gfs,x);

  // typedef  Dune::PDELab::ISTLBackend_BCGS_AMG_SSOR<GO> LS;
  // LS ls(gfs,5000,3);
  typedef Dune::PDELab::ISTLBackend_OVLP_CG_SSORk<GFS,CC> LS;
  LS ls(gfs,cc,10,5,1);
  typedef Dune::PDELab::StationaryLinearProblemSolver<GO,LS,V> SLP;
  SLP slp(go,x,ls,1e-6);
  slp.apply();

  // make discrete function object
  typedef Dune::PDELab::DiscreteGridFunction<GFS,V> DGF;
  DGF dgf(gfs,x);
*/
}

template<typename GV, typename RF>
class Parameter
{
  typedef Dune::PDELab::ConvectionDiffusionBoundaryConditions::Type BCType;

public:
  typedef Dune::PDELab::ConvectionDiffusionParameterTraits<GV,RF> Traits;

  //! tensor diffusion coefficient
  typename Traits::PermTensorType
  A (const typename Traits::ElementType& e, const typename Traits::DomainType& x) const
  {
    typename Traits::PermTensorType I;
    for (std::size_t i=0; i<Traits::dimDomain; i++)
      for (std::size_t j=0; j<Traits::dimDomain; j++)
        I[i][j] = (i==j) ? 1 : 0;
    return I;
  }

  //! velocity field
  typename Traits::RangeType
  b (const typename Traits::ElementType& e, const typename Traits::DomainType& x) const
  {
    typename Traits::RangeType v(0.0);
    return v;
  }

  //! sink term
  typename Traits::RangeFieldType 
  c (const typename Traits::ElementType& e, const typename Traits::DomainType& x) const
  {
    return 0.0;
  }

  //! source term
  typename Traits::RangeFieldType 
  f (const typename Traits::ElementType& e, const typename Traits::DomainType& x) const
  {
    typename Traits::DomainType xglobal = e.geometry().global(x);
    typename Traits::RangeFieldType norm = xglobal.two_norm2();
    return (2.0*GV::dimension-4.0*norm)*exp(-norm); 
  }

  //! boundary condition type function
  BCType
  bctype (const typename Traits::IntersectionType& is, const typename Traits::IntersectionDomainType& x) const
  {
    return Dune::PDELab::ConvectionDiffusionBoundaryConditions::Dirichlet;
  }

  //! Dirichlet boundary condition value
  typename Traits::RangeFieldType 
  g (const typename Traits::ElementType& e, const typename Traits::DomainType& x) const
  {
    typename Traits::DomainType xglobal = e.geometry().global(x);
    typename Traits::RangeFieldType norm = xglobal.two_norm2();
    return exp(-norm);
  }

  //! Neumann boundary condition
  typename Traits::RangeFieldType 
  j (const typename Traits::IntersectionType& is, const typename Traits::IntersectionDomainType& x) const
  {
    return 0.0;
  }

  //! outflow boundary condition
  typename Traits::RangeFieldType 
  o (const typename Traits::IntersectionType& is, const typename Traits::IntersectionDomainType& x) const
  {
    return 0.0;
  }
};


//! solve problem with DG method
template<class GV, class FEM, class PROBLEM, int degree, int blocksize>
void runDG ( const GV& gv, 
             const FEM& fem, 
             PROBLEM& problem,
             std::string basename, 
             int level, 
             std::string method, 
             std::string weights, 
             double alpha )
{
  // coordinate and result type
  typedef typename GV::Grid::ctype Coord;
  typedef double Real;
  const int dim = GV::Grid::dimension;

  std::stringstream fullname;
  fullname << basename << "_" << method << "_w" << weights << "_k" << degree << "_dim" << dim << "_level" << level;

  // make grid function space 
  typedef Dune::PDELab::P0ParallelConstraints CON;
  //const Dune::PDELab::ISTLParameters::Blocking blocking
  //  = Dune::PDELab::ISTLParameters::static_blocking;
  //typedef Dune::PDELab::ISTLVectorBackend<blocking,blocksize> VBE;

  typedef Dune::PDELab::istl::BlockVectorBackend<
    Dune::Memory::blocked_cache_aligned_allocator<double,std::size_t,16>
    > VBE;
  VBE vbe(blocksize);

  typedef Dune::PDELab::DefaultLeafOrderingTag OrderingTag;
  OrderingTag orderingTag;

  /*
  typedef Dune::PDELab::ordering::Permuted<
    Dune::PDELab::DefaultLeafOrderingTag
    > OrderingTag;

  OrderingTag orderingTag;
  std::size_t black_offset;

  std::tie(orderingTag.template permuted<1>().permutation(),black_offset) = Dune::PDELab::redBlackDGOrdering(gv);

  std::cout << "offset of black partition: " << black_offset << std::endl;
  */

  typedef Dune::PDELab::GridFunctionSpace<GV,FEM,CON,VBE,OrderingTag> GFS;
  GFS gfs(gv,fem,vbe,orderingTag);

  // make local operator
  Dune::PDELab::ConvectionDiffusionDGMethod::Type m;
  if (method=="SIPG") m = Dune::PDELab::ConvectionDiffusionDGMethod::SIPG;
  if (method=="NIPG") m = Dune::PDELab::ConvectionDiffusionDGMethod::NIPG;
  Dune::PDELab::ConvectionDiffusionDGWeights::Type w;
  if (weights=="ON") w = Dune::PDELab::ConvectionDiffusionDGWeights::weightsOn;
  if (weights=="OFF") w = Dune::PDELab::ConvectionDiffusionDGWeights::weightsOff;
  typedef Dune::PDELab::ConvectionDiffusionDG<PROBLEM,FEM> LOP;
  LOP lop(problem,m,w,alpha);

  typedef typename Dune::PDELab::istl::BELLMatrixBackend<
    Dune::Memory::blocked_cache_aligned_allocator<double,std::size_t,16>
    > MBE;

  typedef Dune::PDELab::ConvectionDiffusionDirichletExtensionAdapter<PROBLEM> G;
  G g(gv,problem);
  typedef typename GFS::template ConstraintsContainer<Real>::Type CC;
  CC cc;
  Dune::PDELab::constraints(g,gfs,cc,false);
  typedef Dune::PDELab::GridOperator<GFS,GFS,LOP,MBE,Real,Real,Real,CC,CC> GO;
  GO go(gfs,cc,gfs,cc,lop);

  // make a vector of degree of freedom vectors and initialize it with Dirichlet extension
  typedef typename GO::Traits::Domain U;
  U u(gfs,0.0);
  raw(u).setChunkSize(1024);

  U r(gfs,0.0);
  raw(r).setChunkSize(1024);
  go.residual(u,r);

  typename GO::Traits::Jacobian mat(go);
  raw(mat).setChunkSize(1024);
  go.jacobian(u,mat);

  // make linear solver and solve problem
  int verbose=2;
  if (gv.comm().rank()!=0) verbose=0;

  typedef Dune::PDELab::OverlappingOperator<
    CC,
    typename GO::Traits::Jacobian,
    typename GO::Traits::Domain,
    typename GO::Traits::Range
    > POP;
  POP pop(cc,mat);

  Dune::PDELab::istl::ParallelHelper<GFS> helper(gfs);

  typedef Dune::PDELab::OverlappingScalarProduct<
    GFS,
    typename GO::Traits::Domain
    > PSP;
  PSP psp(gfs,helper);

  /*
  typedef Dune::ISTL::SequentialRedBlackBlockSOR<
    typename GO::Traits::Jacobian::Container,
    typename U::Container,
    typename U::Container
    > PC;
    PC pc(raw(mat),black_offset,1.0,false,5);*/

  typedef Dune::ISTL::SequentialBlockJacobi<
    typename GO::Traits::Jacobian::Container,
    typename U::Container,
    typename U::Container
    > PC;
    PC pc(raw(mat),1.0,false,5);

  typedef Dune::PDELab::OverlappingWrappedPreconditioner<
    CC,
    GFS,
    PC
    > PPC;
  PPC ppc(gfs,pc,cc,helper);

  U z(gfs,0.0);
  raw(z).setChunkSize(1024);

  int verbosity = gfs.gridView().comm().rank() == 0 ? 2 : 0;

  if (method=="SIPG")
    {
      typedef Dune::CGSolver<U> Solver;

      Solver solver(pop,psp,ppc,1e-10,500,verbosity);


      Dune::InverseOperatorResult stat;
      solver.apply(z,r,stat);
    }
  else
    {
      typedef Dune::BiCGSTABSolver<U> Solver;
      Solver solver(pop,psp,ppc,1e-10,500,verbosity);

      Dune::InverseOperatorResult stat;
      solver.apply(z,r,stat);
    }

  u -= z;

  if( graphics ){
    typedef Dune::PDELab::DiscreteGridFunction<GFS,U> UDGF;
    UDGF udgf(gfs,u);
    Dune::SubsamplingVTKWriter<GV> vtkwriter(gv,std::max(0,degree-1));
    vtkwriter.addVertexData(new Dune::PDELab::VTKGridFunctionAdapter<UDGF>(udgf,"u_h"));
    vtkwriter.write(fullname.str(),Dune::VTK::appendedraw);
  }

}



int main(int argc, char** argv)
{
  //Maybe initialize Mpi
  Dune::MPIHelper& helper = Dune::MPIHelper::instance(argc, argv);
  if(Dune::MPIHelper::isFake)
    std::cout<< "This is a sequential program." << std::endl;
  else
    {
      if(helper.rank()==0)
        std::cout << "parallel run on " << helper.size() << " process(es)" << std::endl;
    }

  if (argc!=8 && argc!=5) {
    if(helper.rank()==0) {
      std::cout << "usage option 1: " << argv[0] << " <order> <nx> <ny> <nz>" << std::endl;
      std::cout << "usage option 2: " << argv[0] << " <order> <nx> <ny> <nz> <px> <py> <pz>" << std::endl;
    }
    return 0;
  }
  int degree_dyn; sscanf(argv[1],"%d",&degree_dyn);
  int nx; sscanf(argv[2],"%d",&nx);
  int ny; sscanf(argv[3],"%d",&ny);
  int nz; sscanf(argv[4],"%d",&nz);

  int px=0; int py=0; int pz=0;
  if (argc==8){
    sscanf(argv[5],"%d",&px);
    sscanf(argv[6],"%d",&py);
    sscanf(argv[7],"%d",&pz);
  }

  try
    {
      const int dim = 3;
      Dune::FieldVector<double,dim> L(1.0);
      Dune::FieldVector<int,dim> N;
      N[0] = nx; N[1] = ny; N[2] = nz;
      Dune::FieldVector<bool,dim> B(false);
      int overlap=1;


      typedef YaspPartition<dim,Dune::FieldVector<int,dim>> YP;
      YP* yp = (YP*) Dune::YaspGrid<dim>::defaultLoadbalancer();
      if( px*py*pz==0 ){
        // If px,py,pz were not specified choose the default load balancer        
        if( helper.rank() == 0 )
          std::cout << "Using default partitioning of YASP." << std::endl;
      }
      
      else if( px*py*pz != helper.size() ){
        // If px*py*pz is not equal to the available number of processors
        // wrong input, stop and output warning!
        if( helper.rank()==0 )
          std::cerr << "Wrong input: px*py*pz != np" << std::endl;
        exit(1);
      }

      else {
        Dune::FieldVector<int,dim> yasppartitions;
        yasppartitions[0] = px; 
        yasppartitions[1] = py; 
        yasppartitions[2] = pz;
        yp = new YP(yasppartitions);
        if( helper.rank() == 0 )
          std::cout << "Partitioning of YASP: " << yasppartitions << std::endl;
      }

      Dune::YaspGrid<dim> grid(helper.getCommunicator(),L,N,B,overlap,yp);

      typedef Dune::YaspGrid<dim> Grid;
      typedef Grid::LeafGridView GV;

      const GV& gv=grid.leafView();
      typedef Parameter<GV,double> PROBLEM;
      PROBLEM problem;

      if (degree_dyn==0) {
        test(gv);
      }
      if (degree_dyn==1) {
        const int degree=1;
        typedef Dune::PDELab::QkDGLocalFiniteElementMap<Grid::ctype,double,degree,dim> FEMDG;
        FEMDG femdg;
        const int blocksize = Dune::QkStuff::QkSize<degree,dim>::value;
        runDG<GV,FEMDG,PROBLEM,degree,blocksize>(gv,femdg,problem,"CUBE",0,"SIPG","ON",2.0);
      }
      if (degree_dyn==2) {
        const int degree=2;
        typedef Dune::PDELab::QkDGLocalFiniteElementMap<Grid::ctype,double,degree,dim> FEMDG;
        FEMDG femdg;
        const int blocksize = Dune::QkStuff::QkSize<degree,dim>::value;
        runDG<GV,FEMDG,PROBLEM,degree,blocksize>(gv,femdg,problem,"CUBE",0,"SIPG","ON",2.0);
      }
      if (degree_dyn==3) {
        const int degree=3;
        typedef Dune::PDELab::QkDGLocalFiniteElementMap<Grid::ctype,double,degree,dim> FEMDG;
        FEMDG femdg;
        const int blocksize = Dune::QkStuff::QkSize<degree,dim>::value;
        runDG<GV,FEMDG,PROBLEM,degree,blocksize>(gv,femdg,problem,"CUBE",0,"SIPG","ON",2.0);
      }
    }
  catch (Dune::Exception &e)
    {
      std::cerr << "Dune reported error: " << e << std::endl;
      return 1;
    }
  catch (...)
    {
      std::cerr << "Unknown exception thrown!" << std::endl;
      return 1;
    }
}
