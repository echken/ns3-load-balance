/* -*-  Mode: C++; c-file-style: "gnu"; indent-tabs-mode:nil; -*- */
/*
 * Copyright (c) 2011 Centre Tecnologic de Telecomunicacions de Catalunya (CTTC)
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation;
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 * Author: Marco Miozzo <marco.miozzo@cttc.es>
 *         Nicola Baldo <nbaldo@cttc.es>
 * 
 */

#include "ns3/propagation-loss-model.h"
#include "ns3/log.h"
#include "ns3/mobility-model.h"
#include "ns3/double.h"
#include "ns3/pointer.h"
#include <math.h>
#include "hybrid-buildings-propagation-loss-model.h"
#include "ns3/buildings-mobility-model.h"
#include "ns3/enum.h"


NS_LOG_COMPONENT_DEFINE ("HybridBuildingsPropagationLossModel");

namespace ns3 {

NS_OBJECT_ENSURE_REGISTERED (HybridBuildingsPropagationLossModel);



HybridBuildingsPropagationLossModel::HybridBuildingsPropagationLossModel ()
  : BuildingsPropagationLossModel ()
{
}

HybridBuildingsPropagationLossModel::~HybridBuildingsPropagationLossModel ()
{
}

TypeId
HybridBuildingsPropagationLossModel::GetTypeId (void)
{
  static TypeId tid = TypeId ("ns3::HybridBuildingsPropagationLossModel")
  
  .SetParent<BuildingsPropagationLossModel> ()
  
  .AddConstructor<HybridBuildingsPropagationLossModel> ();
  
  return tid;
}


double
HybridBuildingsPropagationLossModel::GetLoss (Ptr<MobilityModel> a, Ptr<MobilityModel> b) const
{
  NS_ASSERT_MSG ((a->GetPosition ().z >= 0) && (b->GetPosition ().z >= 0), "HybridBuildingsPropagationLossModel does not support underground nodes (placed at z < 0)");

  
  double distance = a->GetDistanceFrom (b);
  if (distance <= m_minDistance)
    {
      return 0.0;
    }

  // get the BuildingsMobilityModel pointers
  Ptr<BuildingsMobilityModel> a1 = DynamicCast<BuildingsMobilityModel> (a);
  Ptr<BuildingsMobilityModel> b1 = DynamicCast<BuildingsMobilityModel> (b);
  NS_ASSERT_MSG ((a1 != 0) && (b1 != 0), "HybridBuildingsPropagationLossModel only works with BuildingsMobilityModel");

  double loss = 0.0;

  if (a1->IsOutdoor ())
    {
      if (b1->IsOutdoor ())
        {
          if (distance > 1000)
            {
              NS_LOG_INFO (this << a1->GetPosition ().z << b1->GetPosition ().z << m_rooftopHeight);
              if ((a1->GetPosition ().z < m_rooftopHeight)
                  && (b1->GetPosition ().z < m_rooftopHeight))
                {
                  loss = ItuR1411 (a1, b1);
                  NS_LOG_INFO (this << " 0-0 (>1000): below rooftop -> ITUR1411 : " << loss);
                }
              else
                {
                  // Over the rooftop tranmission -> Okumura Hata
                  loss = OkumuraHata (a1, b1);
                  NS_LOG_INFO (this << " O-O (>1000): above rooftop -> OH : " << loss);
                }
            }
          else
            {
              // short range outdoor communication
              loss = ItuR1411 (a1, b1);
              NS_LOG_INFO (this << " 0-0 (<1000) Street canyon -> ITUR1411 : " << loss);
            }
        }
      else
        {
          // b indoor
          if (distance > 1000)
            {
              if ((a1->GetPosition ().z < m_rooftopHeight)
                  && (b1->GetPosition ().z < m_rooftopHeight))
                {                  
                  loss = ItuR1411 (a1, b1) + ExternalWallLoss (b1) + HeightLoss (a1);
                  NS_LOG_INFO (this << " 0-I (>1000): below rooftop -> ITUR1411 : " << loss);
                }
              else
                {
                  loss = OkumuraHata (a1, b1) + ExternalWallLoss (b1);
                  NS_LOG_INFO (this << " O-I (>1000): above the rooftop -> OH : " << loss);
                }
            }
          else
            {
              loss = ItuR1411 (a1, b1) + ExternalWallLoss (b1) + HeightLoss (b1);
              NS_LOG_INFO (this << " 0-I (<1000) ITUR1411 + BEL : " << loss);
            }
        } // end b1->isIndoor ()
    }
  else
    {
      // a is indoor
      if (b1->IsIndoor ())
        {
          if (a1->GetBuilding () == b1->GetBuilding ())
            {
              // nodes are in same building -> indoor communication ITU-R P.1238
              loss = ItuR1238 (a1, b1) + InternalWallsLoss (a1, b1);;
              NS_LOG_INFO (this << " I-I (same building) ITUR1238 : " << loss);

            }
          else
            {
              // nodes are in different buildings
              loss = ItuR1411 (a1, b1) + ExternalWallLoss (a1) + ExternalWallLoss (b1);
              NS_LOG_INFO (this << " I-I (different) ITUR1238 + 2*BEL : " << loss);
            }
        }
      else
        {
          // b is outdoor
          if (distance > 1000)
            {
              if ((a1->GetPosition ().z < m_rooftopHeight)
                  && (b1->GetPosition ().z < m_rooftopHeight))
                {
                  loss = ItuR1411 (a1, b1) + ExternalWallLoss (a1) + HeightLoss (a1);
                  NS_LOG_INFO (this << " I-O (>1000): down rooftop -> ITUR1411 : " << loss);
                }
              else
                {
                  // above rooftop -> OH
                  loss = OkumuraHata (a1, b1) + ExternalWallLoss (a1) + HeightLoss (a1);
                  NS_LOG_INFO (this << " =I-O (>1000) over rooftop OH + BEL + HG: " << loss);
                }
            }
          else
            {
              loss = ItuR1411 (a1, b1) + ExternalWallLoss (a1)  + HeightLoss (a1);
              NS_LOG_INFO (this << " I-O (<1000)  ITUR1411 + BEL + HG: " << loss);
            }
        } // end b1->IsIndoor ()
    } // end a1->IsOutdoor ()

  return loss;
}


} // namespace ns3