/*
  @copyright Steve Keen 2012
  @author Russell Standish
  This file is part of Minsky.

  Minsky is free software: you can redistribute it and/or modify it
  under the terms of the GNU General Public License as published by
  the Free Software Foundation, either version 3 of the License, or
  (at your option) any later version.

  Minsky is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with Minsky.  If not, see <http://www.gnu.org/licenses/>.
*/
// Implementations of canvas items representing operations and variables.

// invert the display of the power operator so that y is on top, and x
// below, for ticket #327
#define DISPLAY_POW_UPSIDE_DOWN


#include <boost/geometry/geometry.hpp>
#include "cairoItems.h"
#include "operation.h"
#include "minsky.h"
#include "latexMarkup.h"
#include <arrays.h>
#include <pango.h>
#include <ecolab_epilogue.h>

using namespace ecolab;
using namespace std;
using namespace minsky;
using namespace boost::geometry;

RenderOperation::RenderOperation(const OperationBase& op, cairo_t* cairo):
  op(op), cairo(cairo), hoffs(0)
{
  cairo_t *lcairo=cairo;
  cairo_surface_t* surf=NULL;
  if (!lcairo)
    {
      surf = cairo_image_surface_create(CAIRO_FORMAT_A1, 100,100);
      lcairo = cairo_create(surf);
    }

  const float l=op.l, r=op.r;
  w=0.5*(-l+r);
  h=op.h;

  switch (op.type())
    {
    case OperationType::constant:
    case OperationType::data:
      {
        cairo_text_extents_t bbox;
        const NamedOp& c=dynamic_cast<const NamedOp&>(op);

        Pango pango(lcairo);
        pango.setFontSize(10);
        pango.setMarkup(latexToPango(c.description));
        w=0.5*pango.width()+2; 
        h=0.5*pango.height()+4;
        hoffs=pango.top();
        break;
      }
    case OperationType::integrate:
      {
        const IntOp& i=dynamic_cast<const IntOp&>(op);
        if (i.coupled())
          {
            RenderVariable rv(*i.intVar,cairo);
            w+=i.intVarOffset+rv.width(); 
            h=max(h, rv.height());
          }
        break;
      }
    default: break;
    }
 if (surf) //cleanup temporary surface
    {
      cairo_destroy(lcairo);
      cairo_surface_destroy(surf);
    }
}

Polygon RenderOperation::geom() const
{
  Rotate rotate(op.rotation, op.x(), op.y());
  float zl=op.l*op.zoomFactor, zh=op.h*op.zoomFactor, 
    zr=op.r*op.zoomFactor;
  Polygon r;
  // TODO: handle bound integration variables, and constants
  r+= rotate(op.x()+zl, op.y()-zh), rotate(op.x()+zl, op.y()+zh), 
    rotate(op.x()+zr, op.y());
  correct(r);
  return r;
}

void RenderOperation::draw()
{
  op.draw(cairo);
}

namespace
{
  cairo::Surface dummySurf(cairo_image_surface_create(CAIRO_FORMAT_A1, 100,100));
}

RenderVariable::RenderVariable(const VariableBase& var, cairo_t* cairo):
  Pango(cairo? cairo: dummySurf.cairo()), var(var), cairo(cairo)
{
  setFontSize(12);
  if (var.type()==VariableType::constant)
    {
      try
        {
          auto val=var.engExp();
          if (val.engExp==-3) val.engExp=0; //0.001-1.0
          setMarkup(var.mantissa(val)+var.expMultiplier(val.engExp));
        }
      catch (error)
        {
          setMarkup("0");
        }
      w=0.5*Pango::width();
      h=0.5*Pango::height();
    }
  else
    {
      setMarkup(latexToPango(var.name()));
      w=0.5*Pango::width()+12; // enough space for numerical display 
      h=0.5*Pango::height()+4;
    }
  hoffs=Pango::top();
}

Polygon RenderVariable::geom() const
{
  float x=var.x(), y=var.y();
  float wz=w*var.zoomFactor, hz=h*var.zoomFactor;
  Rotate rotate(var.rotation, x, y);

  Polygon r;
  r+= rotate(x-wz, y-hz), rotate(x-wz, y+hz), 
    rotate(x+wz, y+hz), rotate(x+wz, y-hz);
  correct(r);
  return r;
}

void RenderVariable::draw()
{
  //  updatePortLocs();
  var.draw(cairo);
}

void RenderVariable::updatePortLocs() const
{
  double angle=var.rotation * M_PI / 180.0;
  double x0=w, y0=0, x1=-w+2, y1=0;
  double sa=sin(angle), ca=cos(angle);
  var.ports[0]->moveTo(var.x()+var.zoomFactor*(x0*ca-y0*sa), 
                           var.y()+var.zoomFactor*(y0*ca+x0*sa));
  var.ports[1]->moveTo(var.x()+var.zoomFactor*(x1*ca-y1*sa), 
                           var.y()+var.zoomFactor*(y1*ca+x1*sa));
}

bool RenderVariable::inImage(float x, float y)
{
  float dx=x-var.x(), dy=y-var.y();
  float rx=dx*cos(var.rotation*M_PI/180)-dy*sin(var.rotation*M_PI/180);
  float ry=dy*cos(var.rotation*M_PI/180)+dx*sin(var.rotation*M_PI/180);
  return rx>=-w && rx<=w && ry>=-h && ry <= h;
}

double RenderVariable::handlePos() const
{
  var.initSliderBounds();
  var.adjustSliderBounds();
  return w*(var.value()-0.5*(var.sliderMin+var.sliderMax))/(var.sliderMax-var.sliderMin);
}

void minsky::drawTriangle
(cairo_t* cairo, double x, double y, const cairo::Colour& col, double angle)
{
  cairo_save(cairo);
  cairo_new_path(cairo);
  cairo_set_source_rgba(cairo,col.r,col.g,col.b,col.a);
  cairo_translate(cairo,x,y);
  cairo_rotate(cairo, angle);
  cairo_move_to(cairo,10,0);
  cairo_line_to(cairo,0,-3);
  cairo_line_to(cairo,0,3);
  cairo_fill(cairo);
  cairo_restore(cairo);
}

