// driver now parametrized by M, B, G, J
template<typename GV, typename M, typename B, typename G, typename J>
void e02_P1 (const GV& gv, const M& m, const B& b, const G& g,
             const J& j, std::string gridName)
{
  // <<<1>>> Choose domain and range field type
  typedef typename GV::Grid::ctype Coord;
  typedef double Real;
  const int dim = GV::dimension;

  // <<<2a>>> Make grid function space
  typedef Dune::PDELab::P1LocalFiniteElementMap<Coord,Real,dim> FEM;
  FEM fem;
  typedef Dune::PDELab::ConformingDirichletConstraints CON;
  CON con;
  typedef Dune::PDELab::ISTLVectorBackend<1> VBE;
  typedef Dune::PDELab::GridFunctionSpace<GV,FEM,CON,VBE> GFS;
  GFS gfs(gv,fem);

  // <<<2b>>> Compute constraints on function space
  typedef typename GFS::template ConstraintsContainer<Real>::Type CC;
  CC cc;
  Dune::PDELab::constraints(b,gfs,cc);
  std::cout << "constrained dofs=" << cc.size()
            << " of " << gfs.globalSize() << std::endl;

  // <<<3>>> Make FE function extending Dirichlet boundary conditions
  typedef typename GFS::template VectorContainer<Real>::Type V;
  V x(gfs,0.0);
  Dune::PDELab::interpolate(g,gfs,x);

  // <<<4>>> Make grid operator space
  typedef E02LocalOperator<M,B,J> LOP;
  LOP lop(m, b, j);
  typedef Dune::PDELab::ISTLBCRSMatrixBackend<1,1> MBE;
  typedef Dune::PDELab::GridOperatorSpace<GFS,GFS,LOP,CC,CC,MBE> GOS;
  GOS gos(gfs,cc,gfs,cc,lop);

  // <<<5>>> Select a linear solver backend
#if HAVE_SUPERLU
  typedef Dune::PDELab::ISTLBackend_SEQ_SuperLU LS;
  LS ls(true);
#else
  typedef Dune::PDELab::ISTLBackend_SEQ_BCGS_SSOR LS;
  LS ls(5000,true);
#endif

  // <<<6>>> assemble and solve linear problem
  Dune::PDELab::StationaryLinearProblemSolver<GOS,LS,V> slp(gos,x,ls,1e-10);
  slp.apply();

  // <<<7>>> graphical output
  {
    typedef Dune::PDELab::DiscreteGridFunction<GFS,V> DGF;
    DGF xdgf(gfs,x);
    Dune::VTKWriter<GV> vtkwriter(gv,Dune::VTKOptions::conforming);
    vtkwriter.addVertexData(new Dune::PDELab::VTKGridFunctionAdapter<DGF>(xdgf,"solution"));
    vtkwriter.write(gridName.c_str(), Dune::VTKOptions::binaryappended);
  }
}
