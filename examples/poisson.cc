// -*- tab-width: 4; indent-tabs-mode: nil -*-
#ifdef HAVE_CONFIG_H
#include "config.h"     
#endif
#include<iostream>
#include<vector>
#include<map>
#include<string>
#include<dune/common/mpihelper.hh>
#include<dune/common/exceptions.hh>
#include<dune/common/fvector.hh>
#include<dune/common/float_cmp.hh>
#include<dune/common/static_assert.hh>
#include<dune/grid/yaspgrid.hh>
#include<dune/grid/io/file/vtk/subsamplingvtkwriter.hh>
#include<dune/istl/bvector.hh>
#include<dune/istl/operators.hh>
#include<dune/istl/solvers.hh>
#include<dune/istl/preconditioners.hh>
#include<dune/istl/io.hh>

#include<dune/pdelab/finiteelementmap/p0fem.hh>
//#include<dune/pdelab/finiteelementmap/p12dfem.hh>
#include<dune/pdelab/finiteelementmap/p1fem.hh>
#include<dune/pdelab/finiteelementmap/pk2dfem.hh>
#include<dune/pdelab/finiteelementmap/pk3dfem.hh>
//#include<dune/pdelab/finiteelementmap/q12dfem.hh>
#include<dune/pdelab/finiteelementmap/q22dfem.hh>
#include<dune/pdelab/finiteelementmap/q1fem.hh>
#include<dune/pdelab/finiteelementmap/conformingconstraints.hh>
#include<dune/pdelab/finiteelementmap/hangingnodeconstraints.hh>
#include<dune/pdelab/gridfunctionspace/gridfunctionspace.hh>
#include<dune/pdelab/gridfunctionspace/gridfunctionspaceutilities.hh>
#include<dune/pdelab/gridfunctionspace/interpolate.hh>
#include<dune/pdelab/gridfunctionspace/constraints.hh>
#include<dune/pdelab/common/function.hh>
#include<dune/pdelab/common/vtkexport.hh>
#include<dune/pdelab/gridoperatorspace/gridoperatorspace.hh>
#include<dune/pdelab/backend/istlvectorbackend.hh>
#include<dune/pdelab/backend/istlmatrixbackend.hh>
#include<dune/pdelab/backend/istlsolverbackend.hh>
#include<dune/pdelab/localoperator/laplacedirichletp12d.hh>
#include<dune/pdelab/localoperator/poisson.hh>

#include "gridexamples.hh"

//===============================================================
//===============================================================
// Solve the Poisson equation
//           - \Delta u = f in \Omega, 
//                    u = g on \partial\Omega_D
//  -\nabla u \cdot \nu = j on \partial\Omega_N
//===============================================================
//===============================================================

//===============================================================
// Define parameter functions f,g,j and \partial\Omega_D/N
//===============================================================

// function for defining the source term
template<typename GV, typename RF>
class F
  : public Dune::PDELab::AnalyticGridFunctionBase<Dune::PDELab::AnalyticGridFunctionTraits<GV,RF,1>,
                                                  F<GV,RF> >
{
public:
  typedef Dune::PDELab::AnalyticGridFunctionTraits<GV,RF,1> Traits;
  typedef Dune::PDELab::AnalyticGridFunctionBase<Traits,F<GV,RF> > BaseT;

  F (const GV& gv) : BaseT(gv) {}
  inline void evaluateGlobal (const typename Traits::DomainType& x, 
							  typename Traits::RangeType& y) const
  {
    if (x[0]>0.25 && x[0]<0.375 && x[1]>0.25 && x[1]<0.375)
      y = 50.0;
    else
      y = 0.0;
    y=0;
  }
};

// boundary grid function selecting boundary conditions 
template<typename GV>
class B
  : public Dune::PDELab::BoundaryGridFunctionBase<Dune::PDELab::
                                                  BoundaryGridFunctionTraits<GV,int,1,
                                                                             Dune::FieldVector<int,1> >,
                                                  B<GV> >
{
  const GV& gv;

public:
  typedef Dune::PDELab::BoundaryGridFunctionTraits<GV,int,1,Dune::FieldVector<int,1> > Traits;
  typedef Dune::PDELab::BoundaryGridFunctionBase<Traits,B<GV> > BaseT;

  B (const GV& gv_) : gv(gv_) {}

  template<typename I>
  inline void evaluate (const Dune::PDELab::IntersectionGeometry<I>& ig, 
                        const typename Traits::DomainType& x,
                        typename Traits::RangeType& y) const
  {  
    Dune::FieldVector<typename GV::Grid::ctype,GV::dimension> 
      xg = ig.geometry().global(x);

    if (xg[1]<1E-6 || xg[1]>1.0-1E-6)
      {
        y = 0; // Neumann
        return;
      }
    if (xg[0]>1.0-1E-6 && xg[1]>0.5+1E-6)
      {
        y = 0; // Neumann
        return;
      }
    y = 1; // Dirichlet
  }

  //! get a reference to the GridView
  inline const GV& getGridView ()
  {
    return gv;
  }
};

// function for Dirichlet boundary conditions and initialization
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

// function for defining the flux boundary condition
template<typename GV, typename RF>
class J
  : public Dune::PDELab::AnalyticGridFunctionBase<Dune::PDELab::AnalyticGridFunctionTraits<GV,RF,1>,
                                                  J<GV,RF> >
{
public:
  typedef Dune::PDELab::AnalyticGridFunctionTraits<GV,RF,1> Traits;
  typedef Dune::PDELab::AnalyticGridFunctionBase<Traits,J<GV,RF> > BaseT;

  J (const GV& gv) : BaseT(gv) {}
  inline void evaluateGlobal (const typename Traits::DomainType& x, 
							  typename Traits::RangeType& y) const
  {
    if (x[1]<1E-6 || x[1]>1.0-1E-6)
      {
        y = 0;
        return;
      }
    if (x[0]>1.0-1E-6 && x[1]>0.5+1E-6)
      {
        y = -5.0;
        return;
      }
  }
};

//===============================================================
// Problem setup and solution 
//===============================================================

// generate a P1 function and output it
template<typename GV, typename FEM, typename CON, int q> 
void poisson (const GV& gv, const FEM& fem, std::string filename, const CON& con = CON())
{
  // constants and types
  typedef typename GV::Grid::ctype DF;
  typedef typename FEM::Traits::LocalFiniteElementType::Traits::
    LocalBasisType::Traits::RangeFieldType R;

  // make function space
  typedef Dune::PDELab::GridFunctionSpace<GV,FEM,CON,
    Dune::PDELab::ISTLVectorBackend<1> > GFS; 
  GFS gfs(gv,fem,con);

  // make constraints map and initialize it from a function
  typedef typename GFS::template ConstraintsContainer<R>::Type C;
  C cg;
  cg.clear();
  typedef B<GV> BType;
  BType b(gv);
  Dune::PDELab::constraints(b,gfs,cg);

  // make coefficent Vector and initialize it from a function
  typedef typename GFS::template VectorContainer<R>::Type V;
  V x0(gfs);
  x0 = 0.0;
  typedef G<GV,R> GType;
  GType g(gv);
  Dune::PDELab::interpolate(g,gfs,x0);
  Dune::PDELab::set_shifted_dofs(cg,0.0,x0);

  // make grid function operator
  typedef F<GV,R> FType;
  FType f(gv);
  typedef J<GV,R> JType;
  JType j(gv);
  typedef Dune::PDELab::Poisson<FType,BType,JType,q> LOP; 
  LOP lop(f,b,j);
  typedef Dune::PDELab::GridOperatorSpace<GFS,GFS,
    LOP,C,C,Dune::PDELab::ISTLBCRSMatrixBackend<1,1> > GOS;
  GOS gos(gfs,cg,gfs,cg,lop);


  // represent operator as a matrix
  typedef typename GOS::template MatrixContainer<R>::Type M;
  M m(gos);
  m = 0.0;

  // For hangingnodes: Interpolate hangingnodes adajcent to dirichlet
  // nodes
  gos.backtransform(x0);
  
  gos.jacobian(x0,m);
  //  Dune::printmatrix(std::cout,m.base(),"global stiffness matrix","row",9,1);

  // evaluate residual w.r.t initial guess
  V r(gfs);
  r = 0.0;

  gos.residual(x0,r);

  // make ISTL solver
  Dune::MatrixAdapter<M,V,V> opa(m);
//   typedef Dune::PDELab::OnTheFlyOperator<V,V,GOS> ISTLOnTheFlyOperator;
//   ISTLOnTheFlyOperator opb(gos);
  Dune::SeqSSOR<M,V,V> ssor(m,1,1.0);
//   Dune::SeqILU0<M,V,V> ilu0(m,1.0);
//   Dune::Richardson<V,V> richardson(1.0);

  Dune::CGSolver<V> solvera(opa,ssor,1E-10,5000,2);
  //  Dune::CGSolver<V> solverb(opb,richardson,1E-10,5000,2);
  Dune::BiCGSTABSolver<V> solverc(opa,ssor,1E-10,5000,2);
  Dune::InverseOperatorResult stat;

  // solve the jacobian system
  r *= -1.0; // need -residual
  V x(gfs,0.0);
  solvera.apply(x,r,stat);

  // For hangingnodes: Set values of hangingnodes to zero
  Dune::PDELab::set_shifted_dofs(cg,0.0,x0);

  x += x0; //affine shift

  // Transform solution into standard basis
  gos.backtransform(x);

  // make discrete function object
  typedef Dune::PDELab::DiscreteGridFunction<GFS,V> DGF;
  DGF dgf(gfs,x);

  // output grid function with VTKWriter
  Dune::VTKWriter<GV> vtkwriter(gv,Dune::VTKOptions::conforming);
  vtkwriter.addVertexData(new Dune::PDELab::VTKGridFunctionAdapter<DGF>(dgf,"solution"));
  vtkwriter.write(filename,Dune::VTKOptions::ascii);
}


//===============================================================
// Main program with grid setup
//===============================================================

int main(int argc, char** argv)
{
  try{
    //Maybe initialize Mpi
    Dune::MPIHelper::instance(argc, argv);

#if HAVE_ALUGRID
    // unit square with uniform refinement
    {
      // make grid 
      ALUUnitSquare grid;
      grid.globalRefine(4);

      // get view
      typedef ALUUnitSquare::LeafGridView GV;
      const GV& gv=grid.leafView(); 
      
      // make finite element map
      typedef GV::Grid::ctype DF;
      typedef double R;
      const int k=3;
      const int q=2*k;
      typedef Dune::PDELab::Pk2DLocalFiniteElementMap<GV,DF,double,k> FEM;
      FEM fem(gv);
      
      // solve problem
      poisson<GV,FEM,Dune::PDELab::ConformingDirichletConstraints,q>(gv,fem,"poisson_ALU_Pk_2d");
    }

    // unit cube with hanging node refinement
    {
      // make grid 
      ALUCubeUnitSquare grid;
      grid.globalRefine(1);
      
      typedef ALUCubeUnitSquare::Codim<0>::Partition<Dune::All_Partition>::LeafIterator 
        Iterator;
      typedef ALUCubeUnitSquare::LeafIntersectionIterator IntersectionIterator;
      typedef ALUCubeUnitSquare::LeafGridView GV;
      typedef ALUCubeUnitSquare::ctype ctype;

      // get view
      const GV& gv=grid.leafView(); 
      
      // Do some random refinement. The result is a grid that may
      // contain multiple hanging nodes per edge.
      for(int i=0; i<4;++i){
        Iterator it = grid.leafbegin<0,Dune::All_Partition>();
        Iterator eit = grid.leafend<0,Dune::All_Partition>();

        for(;it!=eit;++it){
          if((double)rand()/(double)RAND_MAX > 0.6)
            grid.mark(1,*(it));
        }
        grid.preAdapt();
        grid.adapt();
        grid.postAdapt();
      }

      // make finite element map
      typedef GV::Grid::ctype DF;
      typedef double R;
      const int q=2;
      typedef Dune::PDELab::Q1LocalFiniteElementMap<DF,R,3> FEM;
      FEM fem;

      // We need the boundary function for the hanging nodes
      // constraints engine as we have to distinguish between hanging
      // nodes on dirichlet and on neumann boundaries
      typedef B<GV> BType;
      BType b(gv);

      // This is the type of the local constraints assembler that has
      // to be adapted for different local basis spaces and grid types
      typedef Dune::PDELab::HangingNodesConstraintsAssemblers::CubeGridQ1Assembler ConstraintsAssembler;

      // The type of the constraints engine
      typedef Dune::PDELab::HangingNodesDirichletConstraints
        <GV::Grid,ConstraintsAssembler,BType> Constraints;

      // Get constraints engine. We set adaptToIsolateHangingNodes =
      // true as ALU Grid refinement allows for the appearance of
      // multiple hanging nodes per edge.
      Constraints constraints(grid,true,b);
      
      // solve problem
      poisson<GV,FEM,Constraints,q>(gv,fem,"poisson_ALU_Q1_3d",constraints);
    }

#endif

    // ALU Pk 3D test
#if HAVE_ALUGRID
    {
      // make grid
      typedef ALUUnitCube<3> UnitCube;
      UnitCube unitcube;
      unitcube.grid().globalRefine(2);

      // get view
      typedef UnitCube::GridType::LeafGridView GV;
      const GV& gv=unitcube.grid().leafView(); 
 
      // make finite element map
      typedef UnitCube::GridType::ctype DF;
      typedef double R;
      const int k=4;
      const int q=2*k;
      typedef Dune::PDELab::Pk3DLocalFiniteElementMap<GV,DF,R,k> FEM;
      FEM fem(gv);
  
      // solve problem
      poisson<GV,FEM,Dune::PDELab::ConformingDirichletConstraints,q>(gv,fem,"poisson_ALU_Pk_3d");
    }
#endif


    return 0;

    // YaspGrid Q1 2D test
    {
      // make grid
      Dune::FieldVector<double,2> L(1.0);
      Dune::FieldVector<int,2> N(1);
      Dune::FieldVector<bool,2> B(false);
      Dune::YaspGrid<2> grid(L,N,B,0);
      grid.globalRefine(6);

      // get view
      typedef Dune::YaspGrid<2>::LeafGridView GV;
      const GV& gv=grid.leafView(); 

      // make finite element map
      typedef GV::Grid::ctype DF;
      typedef Dune::PDELab::Q1LocalFiniteElementMap<DF,double,2> FEM;
      FEM fem;
  
      // solve problem
      poisson<GV,FEM,Dune::PDELab::ConformingDirichletConstraints,2>(gv,fem,"poisson_yasp_Q1_2d");
    }

    // YaspGrid Q2 2D test
    {
      // make grid
      Dune::FieldVector<double,2> L(1.0);
      Dune::FieldVector<int,2> N(1);
      Dune::FieldVector<bool,2> B(false);
      Dune::YaspGrid<2> grid(L,N,B,0);
      grid.globalRefine(3);

      // get view
      typedef Dune::YaspGrid<2>::LeafGridView GV;
      const GV& gv=grid.leafView(); 

      // make finite element map
      typedef GV::Grid::ctype DF;
      typedef Dune::PDELab::Q22DLocalFiniteElementMap<DF,double> FEM;
      FEM fem;
  
      // solve problem
      poisson<GV,FEM,Dune::PDELab::ConformingDirichletConstraints,2>(gv,fem,"poisson_yasp_Q2_2d");
    }

    // YaspGrid Q1 3D test
    {
      // make grid
      Dune::FieldVector<double,3> L(1.0);
      Dune::FieldVector<int,3> N(1);
      Dune::FieldVector<bool,3> B(false);
      Dune::YaspGrid<3> grid(L,N,B,0);
      grid.globalRefine(3);

      // get view
      typedef Dune::YaspGrid<3>::LeafGridView GV;
      const GV& gv=grid.leafView(); 

      // make finite element map
      typedef GV::Grid::ctype DF;
      typedef Dune::PDELab::Q1LocalFiniteElementMap<DF,double,3> FEM;
      FEM fem;
  
      // solve problem
      poisson<GV,FEM,Dune::PDELab::ConformingDirichletConstraints,2>(gv,fem,"poisson_yasp_Q1_3d");
    }

    // UG Pk 2D test
#if HAVE_UG

    {
      // make grid 
      UGUnitSquare grid;
      grid.globalRefine(4);

      // get view
      typedef UGUnitSquare::LeafGridView GV;
      const GV& gv=grid.leafView(); 
 
      // make finite element map
      typedef GV::Grid::ctype DF;
      typedef double R;
      const int k=3;
      const int q=2*k;
      typedef Dune::PDELab::Pk2DLocalFiniteElementMap<GV,DF,double,k> FEM;
      FEM fem(gv);
  
      // solve problem
      poisson<GV,FEM,Dune::PDELab::ConformingDirichletConstraints,q>(gv,fem,"poisson_UG_Pk_2d");
    }

    {
      // get grid and do a single global refine
      UGUnitCube<3,1> ugunitcube;
      typedef UGUnitCube<3,1>::GridType Grid;
      Grid & grid = ugunitcube.grid();
      grid.setRefinementType(Grid::LOCAL);
      grid.setClosureType(Grid::NONE);
      grid.globalRefine(1);

      typedef Grid::Codim<0>::Partition<Dune::All_Partition>::LeafIterator 
        Iterator;
      typedef Grid::LeafIntersectionIterator IntersectionIterator;
      typedef Grid::LeafGridView GV;
      typedef Grid::ctype ctype;
      
      // get view
      const GV& gv=grid.leafView(); 

      // Do some random refinement. The result is a grid that may
      // contain multiple hanging nodes per edge.
      for(int i=0; i<4;++i){
        Iterator it = grid.leafbegin<0,Dune::All_Partition>();
        Iterator eit = grid.leafend<0,Dune::All_Partition>();

        for(;it!=eit;++it){
          if((double)rand()/(double)RAND_MAX > 0.6)
            grid.mark(1,*(it));
        }
        grid.preAdapt();
        grid.adapt();
        grid.postAdapt();
      }
 
      // make finite element map
      typedef GV::Grid::ctype DF;
      typedef double R;
      const int q=2;
      typedef Dune::PDELab::Q1LocalFiniteElementMap<DF,R,3> FEM;
      FEM fem;

      // We need the boundary function for the hanging nodes
      // constraints engine as we have to distinguish between hanging
      // nodes on dirichlet and on neumann boundaries
      typedef B<GV> BType;
      BType b(gv);

      // This is the type of the local constraints assembler that has
      // to be adapted for different local basis spaces and grid types
      typedef Dune::PDELab::HangingNodesConstraintsAssemblers::CubeGridQ1Assembler ConstraintsAssembler;

      // The type of the constraints engine
      typedef Dune::PDELab::HangingNodesDirichletConstraints
        <GV::Grid,ConstraintsAssembler,BType> Constraints;

      // Get constraints engine. We set adaptToIsolateHangingNodes =
      // true and therefore the constructor refines the grid until
      // there are fewer than one hanging node per edge.
      Constraints constraints(grid,true,b);

      // solve problem
      poisson<GV,FEM,Constraints,q>(gv,fem,"poisson_UG_Q1_3d",constraints);
      
    }
#endif


#if HAVE_ALBERTA
    {
      // make grid 
      AlbertaUnitSquare grid;
      grid.globalRefine(8);
      
      // get view
      typedef AlbertaUnitSquare::LeafGridView GV;
      const GV& gv=grid.leafView(); 
      
      // make finite element map
      typedef GV::Grid::ctype DF;
      typedef double R;
      const int k=3;
      const int q=2*k;
      typedef Dune::PDELab::Pk2DLocalFiniteElementMap<GV,DF,double,k> FEM;
      FEM fem(gv);
      
      // solve problem
      poisson<GV,FEM,Dune::PDELab::ConformingDirichletConstraints,q>(gv,fem,"poisson_Alberta_Pk_2d");
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
