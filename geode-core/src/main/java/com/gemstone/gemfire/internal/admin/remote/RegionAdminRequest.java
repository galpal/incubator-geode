/*
 * Licensed to the Apache Software Foundation (ASF) under one or more
 * contributor license agreements.  See the NOTICE file distributed with
 * this work for additional information regarding copyright ownership.
 * The ASF licenses this file to You under the Apache License, Version 2.0
 * (the "License"); you may not use this file except in compliance with
 * the License.  You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
   
   
package com.gemstone.gemfire.internal.admin.remote;

import com.gemstone.gemfire.*;
//import com.gemstone.gemfire.admin.OperationCancelledException;
import com.gemstone.gemfire.admin.RegionNotFoundException;
import com.gemstone.gemfire.cache.*;
//import com.gemstone.gemfire.internal.*;
//import com.gemstone.gemfire.internal.cache.*;
//import com.gemstone.gemfire.distributed.internal.*;
import com.gemstone.gemfire.distributed.DistributedSystem;
import com.gemstone.gemfire.internal.i18n.LocalizedStrings;
//import com.gemstone.gemfire.internal.admin.*;
import java.io.*;
//import java.util.*;

/**
 * A message that is sent to a particular app vm on a distribution manager to
 * make an administration request about a particular region.
 */
public abstract class RegionAdminRequest extends AdminRequest {
  // instance variables
  private String regionName;

  public void setRegionName(String name) {
    this.regionName = name;
  }

  public String getRegionName() {
    return this.regionName;
  }

  /**
   * @throws com.gemstone.gemfire.cache.CacheRuntimeException if no cache created
   */
  protected Region getRegion(DistributedSystem sys) {
    Cache cache = CacheFactory.getInstance(sys);
    Region r = cache.getRegion(regionName);
    if (r == null) {
      throw new RegionNotFoundException(LocalizedStrings.RegionAdminRequest_REGION_0_NOT_FOUND_IN_REMOTE_CACHE_1.toLocalizedString(new Object[] {regionName, cache.getName()}));
    }
    return r;
  }

  @Override
  public void toData(DataOutput out) throws IOException {
    super.toData(out);
    DataSerializer.writeString(this.regionName, out);
  }

  @Override
  public void fromData(DataInput in)
    throws IOException, ClassNotFoundException {
    super.fromData(in);
    this.regionName = DataSerializer.readString(in);
  }
}
