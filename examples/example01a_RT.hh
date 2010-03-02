template<class GV>
void example01a_RT (const GV& gv)
{
  // <<<1>>> Choose domain and range field type
  typedef typename GV::Grid::ctype Coord;
  typedef double Real;
  const int dim = GV::dimension;

  // <<<2a>>> Make grid function space
  typedef Dune::PDELab::RannacherTurek2DLocalFiniteElementMap<Coord,Real> FEM;
  FEM fem;
  typedef Dune::PDELab::NoConstraints CON;
  typedef Dune::PDELab::ISTLVectorBackend<1> VBE;
  typedef Dune::PDELab::GridFunctionSpace<GV,FEM,CON,VBE> GFS;
  GFS gfs(gv,fem);

  // <<<2b>>> Constraints (are empty here)
  typedef typename GFS::template ConstraintsContainer<Real>::Type C;

  // <<<3>>> Make grid operator space
  typedef Example01aLocalOperator LOP; 
  LOP lop;
  typedef Dune::PDELab::ISTLBCRSMatrixBackend<1,1> MBE;
  typedef Dune::PDELab::GridOperatorSpace<GFS,GFS,LOP,C,C,MBE> GOS;
  GOS gos(gfs,gfs,lop);

  // <<<4>>> Select a linear solver backend
#if HAVE_SUPERLU
  typedef Dune::PDELab::ISTLBackend_SEQ_SuperLU LS;
  LS ls(true);
#else
  typedef Dune::PDELab::ISTLBackend_SEQ_BCGS_SSOR LS;
  LS ls(5000,true);
#endif

  // <<<5>>> solve linear problem
  typedef typename GFS::template VectorContainer<Real>::Type V;
  V x(gfs,0.0); // initial value
  Dune::PDELab::StationaryLinearProblemSolver<GOS,LS,V> slp(gos,x,ls,1e-10);
  slp.apply();

  // <<<6>>> graphical output
  {
    typedef Dune::PDELab::DiscreteGridFunction<GFS,V> DGF;
    DGF xdgf(gfs,x);
    Dune::SubsamplingVTKWriter<GV> vtkwriter(gv,2);
    vtkwriter.addVertexData(new Dune::PDELab::VTKGridFunctionAdapter<DGF>(xdgf,"solution"));
    vtkwriter.write("example01_RT",Dune::VTKOptions::binaryappended);
  }
}