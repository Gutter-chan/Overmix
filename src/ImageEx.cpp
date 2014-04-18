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

#include "ImageEx.hpp"
#include "Plane.hpp"
#include "MultiPlaneIterator.hpp"
#include "color.hpp"

#include <stdint.h>
#include <limits>
#include <png.h>
#include <zlib.h>
#include <QtConcurrentMap>

#include <QFileInfo>

using namespace std;

static const double DOUBLE_MAX = std::numeric_limits<double>::max();

void ImageEx::to_grayscale(){
	switch( type ){
		case GRAY: break;
		case RGB:
				//TODO: properly convert to grayscale
				while( planes.size() > 1 )
					planes.pop_back();
			break;
		case YUV:
				while( planes.size() > 1 )
					planes.pop_back();
			break;
	}
	type = GRAY;
}

void process_dump_line( color_type *out, unsigned char* in, unsigned width, uint16_t depth ){
	unsigned byte_count = (depth + 7) / 8;
	double scale = pow( 2.0, depth ) - 1.0;
	if( byte_count == 1 )
		for( unsigned ix=0; ix<width; ++ix, ++out )
			*out = color::from_double( (in[ix]) / scale );
	else
		for( unsigned ix=0; ix<width; ++ix, ++out )
			*out = color::from_double( ((uint16_t*)in)[ix] / scale );
}

bool ImageEx::read_dump_plane( FILE *f ){
	if( !f )
		return false;
	
	unsigned width, height;
	fread( &width, sizeof(unsigned), 1, f );
	fread( &height, sizeof(unsigned), 1, f );
	
	planes.emplace_back( width, height );
	auto& p = planes[planes.size()-1];
	
	uint16_t depth;
	uint16_t config;
	fread( &depth, sizeof(uint16_t), 1, f );
	fread( &config, sizeof(uint16_t), 1, f );
	unsigned byte_count = (depth + 7) / 8;
	
	qDebug( "Size: %dx%d (%d-%d)", width, height, depth, config );
	
	if( config & 0x1 ){
		//Compression is used
		qDebug( "Compression is used" );
		uint32_t lenght;
		fread( &lenght, sizeof(uint32_t), 1, f );
		qDebug( "Read lenght: %d", lenght );
		
		unsigned char* input = new unsigned char[ lenght ];
		fread( input, 1, lenght, f );
		
		uLongf total = height*width*byte_count;
		unsigned char* buffer = new unsigned char[ total ];
		uLongf lenght_2 = lenght;
		if( uncompress( (Bytef*)buffer, &total, (Bytef*)input, lenght_2 ) != Z_OK ){
			qDebug( "uncompress failed :\\" );
			delete[] input;
			delete[] buffer;
			return false;
		}
		qDebug( "uncompressed data" );
		
		//Convert data
		for( unsigned iy=0; iy<height; ++iy ){
			unsigned char* line_buf = buffer + width * byte_count * iy;
			color_type *row = p.scan_line( iy );
			process_dump_line( row, line_buf, width, depth );
		}
		
		delete[] input;
		delete[] buffer;
	}
	else{
		unsigned char* buffer = new unsigned char[ width*byte_count ];
		
		for( unsigned iy=0; iy<height; iy++ ){
			color_type *row = p.scan_line( iy );
			fread( buffer, byte_count, width, f );
			process_dump_line( row, buffer, width, depth );
		}
		
		delete[] buffer;
	}
	
	return true;
}

bool ImageEx::from_dump( const char* path ){
	FILE *f = fopen( path, "rb" );
	if( !f )
		return false;
	
	bool result = true;
	result &= read_dump_plane( f );
	result &= read_dump_plane( f );
	result &= read_dump_plane( f );
	
	fclose( f );
	
	type = YUV;
	
	return result;
}

static int read_chunk_callback_thing( png_structp, png_unknown_chunkp ){
	return 0;
}
bool ImageEx::from_png( const char* path ){
	FILE *f = fopen( path, "rb" );
	if( !f )
		return false;
	
	//Read signature
	unsigned char header[8];
	fread( &header, 1, 8, f );
	
	//Check signature
	if( png_sig_cmp( header, 0, 8 ) ){
		fclose( f );
		return false;
	}
	
	//Start initializing libpng
	png_structp png_ptr = png_create_read_struct( PNG_LIBPNG_VER_STRING, NULL, NULL, NULL );
	if( !png_ptr ){
		png_destroy_read_struct( &png_ptr, NULL, NULL );
		fclose( f );
		return false;
	}
	
	png_infop info_ptr = png_create_info_struct( png_ptr );
	if( !info_ptr ){
		png_destroy_read_struct( &png_ptr, NULL, NULL );
		fclose( f );
		return false;
	}
	
	png_infop end_info = png_create_info_struct( png_ptr );
	if( !end_info ){
		png_destroy_read_struct( &png_ptr, &info_ptr, NULL );
		fclose( f );
		return false;
	}
	
	png_init_io( png_ptr, f );
	png_set_sig_bytes( png_ptr, 8 );
	png_set_read_user_chunk_fn( png_ptr, NULL, read_chunk_callback_thing );
	
	//Finally start reading
	png_read_png( png_ptr, info_ptr, PNG_TRANSFORM_STRIP_16  | PNG_TRANSFORM_STRIP_ALPHA | PNG_TRANSFORM_EXPAND, NULL );
	png_bytep *row_pointers = png_get_rows( png_ptr, info_ptr );
	
	unsigned height = png_get_image_height( png_ptr, info_ptr );
	unsigned width = png_get_image_width( png_ptr, info_ptr );
	
	for( int i=0; i<3; i++ )
		planes.emplace_back( width, height );
	
	for( unsigned iy=0; iy<height; iy++ ){
		color_type* r = planes[0].scan_line( iy );
		color_type* g = planes[1].scan_line( iy );
		color_type* b = planes[2].scan_line( iy );
		for( unsigned ix=0; ix<width; ix++ ){
			r[ix] = color::from_8bit( row_pointers[iy][ix*3 + 0] );
			g[ix] = color::from_8bit( row_pointers[iy][ix*3 + 1] );
			b[ix] = color::from_8bit( row_pointers[iy][ix*3 + 2] );
		}
	}
	
	type = RGB;
	
	return true;
	
	//TODO: cleanup libpng
}


bool ImageEx::from_qimage( const char* path ){
	QImage img( path );
	if( img.isNull() )
		return false;
	
	type = RGB;
	int width = img.width();
	int height = img.height();
	
	if( img.hasAlphaChannel() )
		alpha = Plane( width, height );
	
	for( int i=0; i<3; i++ )
		planes.emplace_back( width, height );
	
	for( int iy=0; iy<height; ++iy ){
		color_type* r = planes[0].scan_line( iy );
		color_type* g = planes[1].scan_line( iy );
		color_type* b = planes[2].scan_line( iy );
		color_type* a = nullptr;
		if( alpha )
			a = alpha.scan_line( iy );
		
		const QRgb* in = (const QRgb*)img.constScanLine( iy );
		
		for( int ix=0; ix<width; ++ix, ++in ){
			*(r++) = color::from_8bit( qRed( *in ) );
			*(g++) = color::from_8bit( qGreen( *in ) );
			*(b++) = color::from_8bit( qBlue( *in ) );
			if( alpha )
				*(a++) = color::from_8bit( qAlpha( *in ) );
		}
	}
	
	return true;
}

bool ImageEx::read_file( const char* path ){
	QString ext = QFileInfo( path ).suffix();
	if( ext == "dump" )
		return initialized = from_dump( path );
	if( ext == "png" )
		return initialized = from_png( path );
	
	return initialized = from_qimage( path );
}

bool ImageEx::create( unsigned width, unsigned height, bool use_alpha ){
	if( initialized )
		return false;
	
	unsigned amount;
	switch( type ){
		case GRAY: amount = 1; break;
		case RGB:
		case YUV: amount = 3; break;
		
		default: return false;
	}
	
	for( unsigned i=0; i<amount; i++ )
		planes.emplace_back( width, height );
	
	if( use_alpha )
		alpha = Plane( width, height );
	
	return initialized = true;
}

QImage ImageEx::to_qimage( YuvSystem system, unsigned setting ){
	if( planes.size() == 0 || !planes[0] )
		return QImage();
	
	//Settings
	bool dither = setting & SETTING_DITHER;
	bool gamma = setting & SETTING_GAMMA;
	bool is_yuv = (type == YUV) && (system != SYSTEM_KEEP);
	
	//Create iterator
	std::vector<PlaneItInfo> info;
	info.push_back( PlaneItInfo( planes[0], 0,0 ) );
	if( type != GRAY ){
		info.push_back( PlaneItInfo( planes[1], 0,0 ) );
		info.push_back( PlaneItInfo( planes[2], 0,0 ) );
	}
	if( alpha_plane() )
		info.push_back( PlaneItInfo( alpha_plane(), 0,0 ) );
	MultiPlaneIterator it( info );
	it.iterate_all();
	
	//Fetch with alpha
	color (MultiPlaneIterator::*pixel)() const = NULL;
	
	if( type == GRAY )
		pixel = ( alpha_plane() ) ? &MultiPlaneIterator::gray_alpha : &MultiPlaneIterator::gray;
	else
		pixel = ( alpha_plane() ) ? &MultiPlaneIterator::pixel_alpha : &MultiPlaneIterator::pixel;
	
	
	color *line = new color[ get_width()+1 ];
	
	//Create image
	QImage img(	it.width(), it.height()
		,	( alpha_plane() ) ? QImage::Format_ARGB32 : QImage::Format_RGB32
		);
	img.fill(0);
	
	//TODO: select function
	
	for( unsigned iy=0; iy<it.height(); iy++, it.next_line() ){
		QRgb* row = (QRgb*)img.scanLine( iy );
		for( unsigned ix=0; ix<it.width(); ix++, it.next_x() ){
			color p = (it.*pixel)();
			if( is_yuv ){
				if( system == SYSTEM_REC709 )
					p = p.rec709_to_rgb( gamma );
				else
					p = p.rec601_to_rgb( gamma );
			}
			
			if( dither )
				p += line[ix];
			
			const double scale = color::WHITE / 255.0;
			color rounded = p / scale;
			
			if( dither ){
				color err = p - ( rounded * scale );
				line[ix] = err / 4;
				line[ix+1] += err / 2;
				if( ix )
					line[ix-1] += err / 4;
			}
			
			rounded.trunc( 255 );
			row[ix] = qRgba( rounded.r, rounded.g, rounded.b, rounded.a );
		}
	}
	
	delete[] line;
	
	return img;
}


double ImageEx::diff( const ImageEx& img, int x, int y ) const{
	//Check if valid
	if( !is_valid() || !img.is_valid() )
		return DOUBLE_MAX;
	
	return planes[0].diff( img[0], x, y );
}

bool ImageEx::is_interlaced() const{
	return planes[0].is_interlaced();
}
void ImageEx::replace_line( ImageEx& img, bool top ){
	for( unsigned i=0; i<max(size(), img.size()); ++i )
		planes[i].replace_line( img[i], top );
	if( alpha && img.alpha_plane() )
		alpha.replace_line( img.alpha_plane(), top );
}
void ImageEx::combine_line( ImageEx& img, bool top ){
	for( unsigned i=0; i<max(size(), img.size()); ++i )
		planes[i].combine_line( img[i], top );
	if( alpha && img.alpha_plane() )
		alpha.combine_line( img.alpha_plane(), top );
}

void ImageEx::crop( unsigned left, unsigned top, unsigned right, unsigned bottom ){
	unsigned width = right + left;
	unsigned height = top + bottom;
	if( type == YUV ){
		planes[0] = *planes[0].crop( left*2, top*2, planes[0].get_width()-width*2, planes[0].get_height()-height*2 );
		planes[1] = *planes[1].crop( left, top, planes[1].get_width()-width, planes[1].get_height()-height );
		planes[2] = *planes[2].crop( left, top, planes[2].get_width()-width, planes[2].get_height()-height );
		if( alpha )
			alpha = *alpha.crop( left*2, top*2, alpha.get_width()-width*2, alpha.get_height()-height*2 );
	}
	else{
		for( auto& plane : planes )
			plane = *plane.crop( left, top, plane.get_width()-width, plane.get_height()-height );
		if( alpha )
			alpha = *alpha.crop( left, top, alpha.get_width()-width, alpha.get_height()-height );
	}
};

MergeResult ImageEx::best_round( const ImageEx& img, int level, double range_x, double range_y, DiffCache *cache ) const{
	//Bail if invalid settings
	if(	level < 1
		||	( range_x < 0.0 || range_x > 1.0 )
		||	( range_y < 0.0 || range_y > 1.0 )
		||	!is_valid()
		||	!img.is_valid()
		)
		return MergeResult(QPoint(),DOUBLE_MAX);
	
	//Make sure cache exists
	DiffCache temp;
	if( !cache )
		cache = &temp;
	
	return planes[0].best_round_sub(
			img[0], level
		,	((int)1 - (int)img.get_width()) * range_x, ((int)get_width() - 1) * range_x
		,	((int)1 - (int)img.get_height()) * range_y, ((int)get_height() - 1) * range_y
		,	cache
		);
}


