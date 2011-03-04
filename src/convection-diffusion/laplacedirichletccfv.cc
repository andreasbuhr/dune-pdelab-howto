// -*- tab-width: 4; indent-tabs-mode: nil -*-
/** \file 
    \brief Solve Laplace equation with cell-centered finite volume method
*/
#ifdef HAVE_CONFIG_H
#include "config.h"     
#endif
#include<iostream>
#include<vector>
#include<map>
#include<dune/common/mpihelper.hh>
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

#include<dune/pdelab/finiteelementmap/p0fem.hh>
#include<dune/pdelab/finiteelementmap/p12dfem.hh>
#include<dune/pdelab/finiteelementmap/pk2dfem.hh>
#include<dune/pdelab/gridfunctionspace/gridfunctionspace.hh>
#include<dune/pdelab/gridfunctionspace/gridfunctionspaceutilities.hh>
#include<dune/pdelab/gridfunctionspace/interpolate.hh>
#include<dune/pdelab/constraints/constraints.hh>
#include<dune/pdelab/constraints/constraintsparameters.hh>
#include<dune/pdelab/common/function.hh>
#include<dune/pdelab/common/vtkexport.hh>
#include<dune/pdelab/gridoperatorspace/gridoperatorspace.hh>
#include<dune/pdelab/localoperator/laplacedirichletccfv.hh>
#include<dune/pdelab/backend/istlvectorbackend.hh>
#include<dune/pdelab/backend/istlmatrixbackend.hh>
#include<dune/pdelab/backend/istlsolverbackend.hh>

#include"../utility/gridexamples.hh"


// define some grid functions to interpolate from
template<typename GV, typename RF>
class G
  : public Dune::PDELab::AnalyticGridFunctionBase<Dune::PDELab::AnalyticGridFunctionTraits<GV,RF,1>,
                                                  G<GV,RF> >
{
public:
  typedef Dune::PDELab::AnalyticGridFunctionTraits<GV,RF,1> Traits;
  typedef Dune::PDELab::AnalyticGridFunctionBase<Traits,G<GV,RF> > BaseT;

  G (const GV& gv) : BaseT(gv) {}
  inline void evaluateGlobal (const typename Traits::DomainType& x, 
							  typename Traits::RangeType& y) const
  {
    typename Traits::DomainType center;
    for (int i=0; i<GV::dimension; i++) center[i] = 0.5;
    center -= x;
	y = exp(-center.two_norm2());
  }
};

// selecting the boundary condition type
class BCTypeParam
  : public Dune::PDELab::DirichletConstraintsParameters /*@\label{bcp:base}@*/
{
public:

  template<typename I>
  bool isDirichlet(
				   const I & intersection,   /*@\label{bcp:name}@*/
				   const Dune::FieldVector<typename I::ctype, I::dimension-1> & coord
				   ) const
  {
	
    //Dune::FieldVector<typename I::ctype, I::dimension>
    //  xg = intersection.geometry().global( coord );
    return true;  // Dirichlet b.c. on all boundaries
  }
};


template<class GV> 
void test (const GV& gv, std::string filename )
{
  typedef typename GV::Grid::ctype DF;
  typedef double RF;
  const int dim = GV::dimension;
  Dune::Timer watch;

  // instantiate finite element maps
  typedef Dune::PDELab::P0LocalFiniteElementMap<DF,RF,dim> FEM;
  FEM fem(Dune::GeometryType(Dune::GeometryType::cube,dim)); // works only for cubes
  
  // make function space
  typedef Dune::PDELab::ISTLVectorBackend<1> VBE;
  typedef Dune::PDELab::GridFunctionSpace<GV,FEM,
    Dune::PDELab::NoConstraints,VBE,
    Dune::PDELab::SimpleGridFunctionStaticSize> GFS; 
  watch.reset();
  GFS gfs(gv,fem);
  std::cout << "=== function space setup " <<  watch.elapsed() << " s" << std::endl;

  // make coefficent Vector and initialize it from a function
  typedef typename Dune::PDELab::BackendVectorSelector<GFS,RF>::Type V;
  V x0(gfs);
  x0 = 0.0;
  typedef G<GV,RF> GType;
  GType g(gv);
  Dune::PDELab::interpolate(g,gfs,x0);

  // make grid function operator
  Dune::PDELab::LaplaceDirichletCCFV<GType> la(g);
  typedef Dune::PDELab::GridOperatorSpace<GFS,GFS,Dune::PDELab::LaplaceDirichletCCFV<GType>,Dune::PDELab::EmptyTransformation,Dune::PDELab::EmptyTransformation,VBE::MatrixBackend > GOS;
  GOS gos(gfs,gfs,la);

  // represent operator as a matrix
  typedef typename GOS::template MatrixContainer<RF>::Type M;
  watch.reset();
  M m(gos);
  std::cout << "=== matrix setup " <<  watch.elapsed() << " s" << std::endl;
  m = 0.0;
  watch.reset();
  gos.jacobian(x0,m);
  std::cout << "=== jacobian assembly " <<  watch.elapsed() << " s" << std::endl;
 //Dune::printmatrix(std::cout,m.base(),"global stiffness matrix","row",9,1);

  // evaluate residual w.r.t initial guess
  V r(gfs);
  r = 0.0;
  watch.reset();
  gos.residual(x0,r);
  std::cout << "=== residual evaluation " <<  watch.elapsed() << " s" << std::endl;

  // make ISTL solver
  Dune::MatrixAdapter<M,V,V> opa(m);
  typedef Dune::PDELab::OnTheFlyOperator<V,V,GOS> ISTLOnTheFlyOperator;
  ISTLOnTheFlyOperator opb(gos);
  Dune::SeqSSOR<M,V,V> ssor(m,1,1.0);
  //  Dune::SeqILU0<M,V,V> ilu0(m,1.0);
  Dune::Richardson<V,V> richardson(1.0);

  typedef typename M::BaseT ISTLM;
  typedef typename V::BaseT ISTLV;
  Dune::MatrixAdapter<ISTLM,ISTLV,ISTLV> opc(m);
  typedef Dune::Amg::CoarsenCriterion<Dune::Amg::SymmetricCriterion<ISTLM,Dune::Amg::FirstDiagonal> > Criterion;
  int maxlevel = 20, coarsenTarget = 100;
  Criterion criterion(maxlevel, coarsenTarget);
  criterion.setMaxDistance(2);

  typedef Dune::SeqSSOR<ISTLM,ISTLV,ISTLV> Smoother;
  typedef typename Dune::Amg::SmootherTraits<Smoother>::Arguments SmootherArgs;
  SmootherArgs smootherArgs;
  smootherArgs.iterations = 1;

  typedef Dune::Amg::AMG<Dune::MatrixAdapter<ISTLM,ISTLV,ISTLV>,ISTLV,Smoother> AMG;
  AMG amg(opc,criterion,smootherArgs,1,1);

  Dune::CGSolver<V> solvera(opa,ssor,1E-10,5000,2);
  Dune::CGSolver<V> solverb(opb,richardson,1E-10,5000,2);
  Dune::CGSolver<ISTLV> solverc(opc,amg,1E-10,5000,2);
  Dune::InverseOperatorResult stat;

  // solve the jacobian system
  r *= -1.0; // need -residual
  V x(gfs,0.0);
  x=1;
  solverc.apply(x,r,stat);
  x += x0;

//   // make discrete function object
  typedef Dune::PDELab::DiscreteGridFunction<GFS,V> DGF;
  DGF dgf(gfs,x);
  
  // output grid function with VTKWriter
  Dune::VTKWriter<GV> vtkwriter(gv,Dune::VTKOptions::nonconforming);
  vtkwriter.addVertexData(new Dune::PDELab::VTKGridFunctionAdapter<DGF>(dgf,"u"));
  vtkwriter.write( filename.c_str() ,Dune::VTKOptions::ascii);
}

int main(int argc, char** argv)
{
  try{
    //Maybe initialize Mpi
    Dune::MPIHelper::instance(argc, argv);

    // 2D
    {
      // make grid
      Dune::FieldVector<double,2> L(1.0);
      Dune::FieldVector<int,2> N(1);
      Dune::FieldVector<bool,2> B(false);
      Dune::YaspGrid<2> grid(L,N,B,0);
      grid.globalRefine(6);
      
      // solve problem :)
      test(grid.leafView(),"laplacedirichletccfv_yasp2d");
    }

    // UG Q1 2D test
#if HAVE_UG
    {
      // make grid 
      UGUnitSquareQ grid(1000);
      grid.globalRefine(6);

      test(grid.leafView(),"laplacedirichletccfv_ug2d");
    }
#endif

	// test passed
	return 0;

  }
  catch (Dune::Exception &e){
    std::cerr << "Dune reported error: " << e << std::endl;
	return 1;
  }
  catch (...){
    std::cerr << "Unknown exception thrown!" << std::endl;
	return 1;
  }
} 
