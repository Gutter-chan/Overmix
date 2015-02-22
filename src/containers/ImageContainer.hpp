/*
	This file is part of Overmix.

	Overmix is free software: you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation, either version 3 of the License, or
	(at your option) any later version.

	Overmix is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with Overmix.  If not, see <http://www.gnu.org/licenses/>.
*/

#ifndef IMAGE_CONTAINER_HPP
#define IMAGE_CONTAINER_HPP

#include "ImageGroup.hpp"

class ImageContainer : public AContainer{
	public:
		/** A index to an ImageItem */
		struct ImagePosition{
			unsigned group;
			unsigned index;
			ImagePosition( unsigned group, unsigned index ) : group(group), index(index) { }
		};
		
	private:
		std::vector<ImageGroup> groups;
		std::vector<Plane> masks;
		std::vector<ImagePosition> indexes;
		
		bool aligned{ false };
		
	public:
		unsigned groupAmount() const{ return groups.size(); }
		const ImageGroup& getConstGroup( unsigned index ) const{ return groups[index]; }
		ImageGroup& getGroup( unsigned index ){ return groups[index]; }
		
		void clear(){
			groups.clear();
			masks.clear();
			indexes.clear();
		}
		
		std::vector<ImageGroup>::iterator        begin()       { return groups.begin(); }
		std::vector<ImageGroup>::const_iterator  begin()  const{ return groups.begin(); }
		std::vector<ImageGroup>::iterator        end()         { return groups.end();   }
		std::vector<ImageGroup>::const_iterator  end()    const{ return groups.end();   }
		
	public: //AContainer implementation
		virtual       unsigned  count() const;
		virtual const ImageEx&     image( unsigned index ) const override;
		virtual       ImageEx&  imageRef( unsigned index ) override;
		virtual const Plane&       alpha( unsigned index ) const override;
		virtual       int      imageMask( unsigned index ) const override;
		virtual const Plane&        mask( unsigned index ) const override{ return masks[index]; }
		virtual       unsigned maskCount()              const override{ return masks.size(); }
		virtual       Point<double>  pos( unsigned index ) const override;
		virtual       void        setPos( unsigned index, Point<double> newVal ) override;
		virtual       int          frame( unsigned index ) const override;
		virtual       void      setFrame( unsigned index, int newVal ) override;
		
	public:
		bool isAligned() const{ return aligned; }
		void setUnaligned(){ aligned = false; }
		void setAligned(){ aligned = true; }
		
		void prepareAdds( unsigned amount );
		ImageItem& addImage( ImageEx&& img, int mask=-1, int group=-1, QString filepath="" );
		
		int addMask( Plane&& mask ){
			masks.emplace_back( mask );
			return masks.size() - 1;
		}
		const std::vector<Plane>& getMasks() const{ return masks; }	
		
		void addGroup( QString name ){ groups.emplace_back( name, masks ); }
		void addGroup( QString name, unsigned group, unsigned from, unsigned to );
		
		void moveImage( unsigned from_group, unsigned from_img
			,	unsigned to_group, unsigned to_img );
		void moveGroup( unsigned from, unsigned to );
		
		void removeImage( unsigned group, unsigned img );
		
		bool removeGroups( unsigned from, unsigned amount );
		void rebuildIndexes();
		
		void onAllItems( void update( ImageItem& item ) );

		template<typename T>
		void onAllItems( T update ){
			for( auto& group : groups )
				for( auto& item : group.items )
					update( item );
		}
		
		//TODO: Get group thing
		
		//TODO: Get render thing
};

#endif