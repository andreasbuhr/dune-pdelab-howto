#ifndef CG_STOKES_INITIAL_HH
#define CG_STOKES_INITIAL_HH

//===============================================================
// Define parameter functions f,g,j and \partial\Omega_D/N
//===============================================================

// constraints parameter class for selecting boundary condition type
class BCTypeParam_HagenPoiseuille
{
public:
  typedef Dune::PDELab::StokesBoundaryCondition BC;
  
  BCTypeParam_HagenPoiseuille() {}
  
  template<typename I>
  inline void evaluate (
    const I & intersection,   /*@\label{bcp:name}@*/
    const Dune::FieldVector<typename I::ctype, I::dimension-1> & coord,
    BC::Type& y) const
  {
    Dune::FieldVector<typename I::ctype, I::dimension>
        xg = intersection.geometry().global( coord );
    if( xg[0] < 1e-6 )
      y = BC::DoNothing;
    else
      y = BC::VelocityDirichlet;
  }
};

// constraints parameter class for selecting boundary condition type 
template<typename GV>
class BCTypeParam_PressureDrop
{
private:
  typedef typename GV::ctype DFT;
  const DFT length;
  const DFT origin;
  const int direction;
  
public:
  typedef Dune::PDELab::StokesBoundaryCondition BC;
  
  BCTypeParam_PressureDrop (const DFT l_, const DFT o_, const int d_)
    : length(l_), origin(o_), direction(d_)
  {
  }

  template<typename I>
  inline void evaluate (
    const I & intersection,   /*@\label{bcp:name}@*/
    const Dune::FieldVector<typename I::ctype, I::dimension-1> & coord,
    BC::Type& y) const
  {
    Dune::FieldVector<typename I::ctype, I::dimension>
      xg = intersection.geometry().global( coord );
    
    if(xg[direction]-origin < 1e-6 || xg[direction]-origin > length-1e-6)
      y = BC::DoNothing;
    else
      y = BC::VelocityDirichlet;
  }
  
};

template<typename GV, typename RF, int dim>
class HagenPoiseuilleVelocity :
  public Dune::PDELab::AnalyticGridFunctionBase<
  Dune::PDELab::AnalyticGridFunctionTraits<GV,RF,dim>,
  HagenPoiseuilleVelocity<GV,RF,dim> >
{
public:
  typedef Dune::PDELab::AnalyticGridFunctionTraits<GV,RF,dim> Traits;
  typedef Dune::PDELab::AnalyticGridFunctionBase<Traits, HagenPoiseuilleVelocity<GV,RF,dim> > BaseT;

  typedef typename Traits::DomainType DomainType;
  typedef typename Traits::RangeType RangeType;

  HagenPoiseuilleVelocity(const GV & gv) : BaseT(gv) {}

  inline void evaluateGlobal(const DomainType & x, RangeType & y) const
  {
    RF r = 0;

    for(int i=1; i<dim; ++i){
      r += (x[i]-0.5)*(x[i]-0.5);
      y[i] = 0;
    }
    r = sqrt(r);
    y[0] = 0.25 - r*r;
  }

};

template<typename GV, typename RF>
class HagenPoiseuilleVelocity<GV,RF,3> :
  public Dune::PDELab::AnalyticGridFunctionBase<
  Dune::PDELab::AnalyticGridFunctionTraits<GV,RF,3>,
  HagenPoiseuilleVelocity<GV,RF,3> >
{
public:
  enum {dim = 3};
  typedef Dune::PDELab::AnalyticGridFunctionTraits<GV,RF,dim> Traits;
  typedef Dune::PDELab::AnalyticGridFunctionBase<Traits, HagenPoiseuilleVelocity<GV,RF,dim> > BaseT;

  typedef typename Traits::DomainType DomainType;
  typedef typename Traits::RangeType RangeType;

  HagenPoiseuilleVelocity(const GV & gv) : BaseT(gv) {}

  inline void evaluateGlobal(const DomainType & x, RangeType & y) const
  {

    RF r = 0;
    for(int i=1; i<dim; ++i){
      r += (x[i])*(x[i]);
      y[i] = 0;
    }
    r = sqrt(r);
    y[0] = 0.25 - r*r;

    if(y[0]<0)
      y[0] = 0;
  }

};


template<typename GV, typename RF>
class ZeroScalarFunction :
  public Dune::PDELab::AnalyticGridFunctionBase<
  Dune::PDELab::AnalyticGridFunctionTraits<GV,RF,1>,
  ZeroScalarFunction<GV,RF> >,
  public Dune::PDELab::InstationaryFunctionDefaults
{
public:
  typedef Dune::PDELab::AnalyticGridFunctionTraits<GV,RF,1> Traits;
  typedef Dune::PDELab::AnalyticGridFunctionBase<Traits, ZeroScalarFunction<GV,RF> > BaseT;

  typedef typename Traits::DomainType DomainType;
  typedef typename Traits::RangeType RangeType;

  ZeroScalarFunction(const GV & gv) : BaseT(gv) {}

  inline void evaluateGlobal(const DomainType & x, RangeType & y) const
  {
    y=0;
  }
};

// function for defining the flux boundary condition
template<typename GV, typename RF>
class HagenPoiseuilleZeroFlux
  : public Dune::PDELab::AnalyticGridFunctionBase<
  Dune::PDELab::AnalyticGridFunctionTraits<GV,RF,1>,
  HagenPoiseuilleZeroFlux<GV,RF> >
{
public:
  typedef Dune::PDELab::AnalyticGridFunctionTraits<GV,RF,1> Traits;
  typedef Dune::PDELab::AnalyticGridFunctionBase<Traits,HagenPoiseuilleZeroFlux<GV,RF> > BaseT;

  HagenPoiseuilleZeroFlux (const GV& gv) : BaseT(gv) {}

  inline void evaluateGlobal (const typename Traits::DomainType& x,
                              typename Traits::RangeType& y) const
  {
    y = 0;
  }
};

// function for defining the flux boundary condition
template<typename GV, typename RF>
class PressureDropFlux
  : public Dune::PDELab::AnalyticGridFunctionBase<
  Dune::PDELab::AnalyticGridFunctionTraits<GV,RF,1>,
  PressureDropFlux<GV,RF> >
{
public:
  typedef Dune::PDELab::AnalyticGridFunctionTraits<GV,RF,1> Traits;
  typedef Dune::PDELab::AnalyticGridFunctionBase<Traits,PressureDropFlux<GV,RF> > BaseT;

private:
  typedef typename Traits::DomainFieldType DFT;
  
  const DFT pressure;
  const DFT length;
  const DFT origin;
  const int direction;

public:
  PressureDropFlux (const GV& gv, const RF p_, const RF l_, const RF o_, const int d_) 
    : BaseT(gv), pressure(p_), length(l_), origin(o_), direction(d_)
  {
    const int dim = GV::dimension;
    assert(direction >=0 && direction <dim);
  }

  inline void evaluateGlobal (const typename Traits::DomainType& x,
                              typename Traits::RangeType& y) const
  {
    if(x[direction]-origin < 1e-6)
      y = pressure;
    else if(x[direction]-origin > length-1e-6)
      y = 0.;
  }
};


#endif
